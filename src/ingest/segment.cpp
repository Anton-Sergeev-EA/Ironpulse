#include "ironpulse/ingest/segment.hpp"

#include <chrono>
#include <cstdio>
#include <ctime>
#include <nlohmann/json.hpp>
#include <stdexcept>
#include <system_error>

namespace ironpulse::ingest {

namespace {

constexpr std::uint64_t align8(std::uint64_t n) noexcept {
    return (n + 7) & ~std::uint64_t{7};
}

std::int64_t now_ms() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::system_clock::now().time_since_epoch())
        .count();
}

/// telemetry-20261002T104311-123Z-000001 — sorts chronologically by name.
std::string segment_stem(std::int64_t created_ms, std::uint64_t sequence) {
    const std::time_t seconds = static_cast<std::time_t>(created_ms / 1000);
    std::tm utc{};
#if defined(_WIN32)
    gmtime_s(&utc, &seconds);
#else
    gmtime_r(&seconds, &utc);
#endif
    char stamp[32];
    std::strftime(stamp, sizeof(stamp), "%Y%m%dT%H%M%S", &utc);
    char name[96];
    std::snprintf(name,
                  sizeof(name),
                  "telemetry-%s-%03dZ-%06llu",
                  stamp,
                  static_cast<int>(created_ms % 1000),
                  static_cast<unsigned long long>(sequence));
    return name;
}

bool ends_with(const std::string& text, const std::string& suffix) {
    return text.size() >= suffix.size() && text.compare(text.size() - suffix.size(), suffix.size(), suffix) == 0;
}

}  // namespace

// ---------------------------------------------------------------------------
// Tag table
// ---------------------------------------------------------------------------

std::string encode_tag_table(const std::vector<TagInfo>& tags) {
    nlohmann::json list = nlohmann::json::array();
    for (const auto& tag : tags) {
        list.push_back({
            {"id", tag.id},
            {"sensor", tag.sensor_id},
            {"device", tag.device_id},
            {"name", tag.name},
            {"unit", tag.unit},
        });
    }
    return nlohmann::json{{"format", "ironpulse-export"}, {"tags", std::move(list)}}.dump();
}

std::vector<TagInfo> decode_tag_table(const std::string& json_text) {
    try {
        const auto doc = nlohmann::json::parse(json_text);
        std::vector<TagInfo> tags;
        for (const auto& item : doc.at("tags")) {
            TagInfo tag;
            tag.id = item.at("id").get<std::uint32_t>();
            tag.sensor_id = item.at("sensor").get<std::string>();
            tag.device_id = item.value("device", "");
            tag.name = item.value("name", "");
            tag.unit = item.value("unit", "");
            tags.push_back(std::move(tag));
        }
        return tags;
    } catch (const nlohmann::json::exception& e) {
        throw std::runtime_error(std::string("malformed tag table: ") + e.what());
    }
}

// ---------------------------------------------------------------------------
// SegmentWriter
// ---------------------------------------------------------------------------

SegmentWriter::SegmentWriter(SegmentWriterOptions options, std::vector<TagInfo> tags)
    : options_(std::move(options)), tag_table_(encode_tag_table(tags)) {
    std::filesystem::create_directories(options_.directory);

    // A *.part file can only be left over from a crash: finalise it so the
    // batches that made it to disk are not silently ignored downstream.
    const std::string partial_ending = std::string(kSegmentExtension) + kPartialSuffix;
    for (const auto& entry : std::filesystem::directory_iterator(options_.directory)) {
        const std::string name = entry.path().filename().string();
        if (entry.is_regular_file() && ends_with(name, partial_ending)) {
            auto finished = entry.path();
            finished.replace_extension();  // drop ".part"
            std::error_code ec;
            std::filesystem::rename(entry.path(), finished, ec);
            if (!ec) {
                ++recovered_;
            }
        }
    }
}

SegmentWriter::~SegmentWriter() {
    close();
}

void SegmentWriter::open_segment() {
    const std::int64_t created = now_ms();
    do {
        ++sequence_;
        final_path_ = options_.directory / (segment_stem(created, sequence_) + kSegmentExtension);
        partial_path_ = final_path_;
        partial_path_ += kPartialSuffix;
    } while (std::filesystem::exists(final_path_) || std::filesystem::exists(partial_path_));

    out_.open(partial_path_, std::ios::binary | std::ios::trunc);
    if (!out_) {
        throw std::runtime_error("cannot create export segment " + partial_path_.string());
    }

    const SegmentHeader header{
        .magic = kSegmentMagic,
        .version = kSegmentVersion,
        .flags = 0,
        .created_ms = created,
        .tag_table_bytes = static_cast<std::uint32_t>(tag_table_.size()),
        .tag_table_crc32c = Serializer::crc32c(tag_table_.data(), tag_table_.size()),
        .reserved = 0,
    };
    out_.write(reinterpret_cast<const char*>(&header), sizeof(header));
    out_.write(tag_table_.data(), static_cast<std::streamsize>(tag_table_.size()));
    const std::uint64_t prefix = sizeof(header) + tag_table_.size();
    const std::uint64_t padding = align8(prefix) - prefix;
    static constexpr char kZeros[8] = {};
    out_.write(kZeros, static_cast<std::streamsize>(padding));
    out_.flush();
    if (!out_) {
        finish_segment();
        throw std::runtime_error("cannot write export segment header to " + partial_path_.string());
    }
    segment_bytes_ = prefix + padding;
    bytes_written_.fetch_add(segment_bytes_, std::memory_order_relaxed);
}

void SegmentWriter::write_batch(std::span<const std::uint8_t> batch) {
    if (!out_.is_open()) {
        open_segment();
    } else if (segment_bytes_ + batch.size() > options_.max_segment_bytes) {
        finish_segment();
        open_segment();
    }

    out_.write(reinterpret_cast<const char*>(batch.data()), static_cast<std::streamsize>(batch.size()));
    out_.flush();
    if (!out_) {
        // Part of the batch may have reached the file. Seal the segment —
        // the reader reports a truncated tail — and start clean next time.
        const auto failed = final_path_;
        finish_segment();
        throw std::runtime_error("write to export segment " + failed.string() + " failed");
    }
    segment_bytes_ += batch.size();
    bytes_written_.fetch_add(batch.size(), std::memory_order_relaxed);
}

void SegmentWriter::finish_segment() noexcept {
    if (!out_.is_open()) {
        return;
    }
    out_.close();
    out_.clear();
    std::error_code ec;
    std::filesystem::rename(partial_path_, final_path_, ec);
    segments_completed_.fetch_add(1, std::memory_order_relaxed);
    segment_bytes_ = 0;
}

void SegmentWriter::close() noexcept {
    finish_segment();
}

// ---------------------------------------------------------------------------
// Reader
// ---------------------------------------------------------------------------

SegmentReadResult read_segment(const std::filesystem::path& path, const BatchVisitor& on_batch) {
    SegmentReadResult result;
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        result.error = "cannot open " + path.string();
        return result;
    }
    std::error_code size_ec;
    const std::uint64_t file_size = std::filesystem::file_size(path, size_ec);
    if (size_ec) {
        result.error = "cannot stat " + path.string();
        return result;
    }

    SegmentHeader header{};
    if (file_size < sizeof(header) || !in.read(reinterpret_cast<char*>(&header), sizeof(header))) {
        result.error = "file is too short to be a segment";
        return result;
    }
    if (header.magic != kSegmentMagic) {
        result.error = "not an Ironpulse export segment (bad magic)";
        return result;
    }
    if (header.version != kSegmentVersion) {
        result.error = "unsupported segment version " + std::to_string(header.version);
        return result;
    }
    if (sizeof(header) + std::uint64_t{header.tag_table_bytes} > file_size) {
        result.error = "tag table extends past the end of the file";
        return result;
    }
    std::string table(header.tag_table_bytes, '\0');
    in.read(table.data(), static_cast<std::streamsize>(table.size()));
    if (!in || Serializer::crc32c(table.data(), table.size()) != header.tag_table_crc32c) {
        result.error = "tag table is damaged";
        return result;
    }
    try {
        result.tags = decode_tag_table(table);
    } catch (const std::exception& e) {
        result.error = e.what();
        return result;
    }
    result.created_ms = header.created_ms;

    std::uint64_t pos = align8(sizeof(header) + table.size());
    std::vector<TelemetrySample> samples;
    std::vector<std::uint8_t> header_bytes(sizeof(BatchHeader));

    auto read_at = [&](std::uint64_t offset, void* dst, std::size_t bytes) {
        in.clear();
        in.seekg(static_cast<std::streamoff>(offset));
        return static_cast<bool>(in.read(static_cast<char*>(dst), static_cast<std::streamsize>(bytes)));
    };

    while (pos < file_size) {
        if (file_size - pos < sizeof(BatchHeader)) {
            result.truncated_tail = true;
            break;
        }
        BatchHeader batch_header{};
        std::size_t batch_bytes = 0;
        read_at(pos, header_bytes.data(), header_bytes.size());
        const auto status = Serializer::peek(header_bytes, batch_header, batch_bytes);

        if (status == DecodeStatus::bad_magic || status == DecodeStatus::unsupported_version) {
            // Lost sync: count the damaged region once, then look for the
            // next batch header on an 8-byte boundary.
            ++result.corrupt_batches;
            pos += 8;
            std::uint32_t magic = 0;
            while (pos + sizeof(BatchHeader) <= file_size) {
                read_at(pos, &magic, sizeof(magic));
                if (magic == Serializer::kMagic) {
                    break;
                }
                pos += 8;
            }
            continue;
        }

        if (pos + batch_bytes > file_size) {
            result.truncated_tail = true;
            break;
        }
        samples.resize(batch_header.count);
        const std::size_t payload_bytes = batch_bytes - sizeof(BatchHeader);
        if (payload_bytes > 0 && !read_at(pos + sizeof(BatchHeader), samples.data(), payload_bytes)) {
            result.truncated_tail = true;
            break;
        }
        if (Serializer::crc32c(samples.data(), payload_bytes) == batch_header.payload_crc32c) {
            ++result.batches;
            result.samples += samples.size();
            if (on_batch) {
                on_batch(samples, result.tags);
            }
        } else {
            ++result.corrupt_batches;
        }
        pos += batch_bytes;
    }
    return result;
}

}  // namespace ironpulse::ingest
