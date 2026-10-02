#pragma once

#include <atomic>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <functional>
#include <span>
#include <string>
#include <vector>

#include "ironpulse/ingest/serializer.hpp"

namespace ironpulse::ingest {

/// Export segment file layout (all integers little-endian):
///
///   SegmentHeader                32 bytes
///   tag table                    tag_table_bytes of UTF-8 JSON
///   zero padding                 up to the next multiple of 8
///   batch, batch, ...            Serializer batches, back to back
///
/// Every batch therefore starts on an 8-byte boundary. The tag table maps
/// the numeric tag_id carried by each sample to the sensor it came from:
///   {"format":"ironpulse-export","tags":[{"id":1,"sensor":"winding_temp",
///    "device":"transformer_01","name":"Winding temperature","unit":"°C"}]}
/// so a segment can be decoded on its own, even after the configuration
/// that produced it has changed.
struct SegmentHeader {
    std::uint32_t magic;  // "IPSG" read as a little-endian uint32
    std::uint16_t version;
    std::uint16_t flags;
    std::int64_t created_ms;  // Unix epoch, milliseconds
    std::uint32_t tag_table_bytes;
    std::uint32_t tag_table_crc32c;
    std::uint64_t reserved;
};
static_assert(sizeof(SegmentHeader) == 32);

inline constexpr std::uint32_t kSegmentMagic = 0x47535049;  // bytes 'I' 'P' 'S' 'G'
inline constexpr std::uint16_t kSegmentVersion = 1;
/// Extension of a finished segment. While being written it carries an
/// extra ".part" suffix, so anything shipping segments elsewhere (rclone,
/// an S3 sync, a cron job) can simply skip *.part files.
inline constexpr const char* kSegmentExtension = ".ipseg";
inline constexpr const char* kPartialSuffix = ".part";

/// What a tag_id stands for.
struct TagInfo {
    std::uint32_t id = 0;
    std::string sensor_id{};
    std::string device_id{};
    std::string name{};
    std::string unit{};
};

[[nodiscard]] std::string encode_tag_table(const std::vector<TagInfo>& tags);
/// Throws std::runtime_error on malformed input.
[[nodiscard]] std::vector<TagInfo> decode_tag_table(const std::string& json_text);

struct SegmentWriterOptions {
    std::filesystem::path directory = "export";
    /// A new segment is started once the current one would grow past this.
    std::uint64_t max_segment_bytes = 64ULL * 1024 * 1024;
};

/// Appends encoded batches to segment files, starting a new segment when
/// the current one is full. Not thread-safe: write_batch() and close() are
/// meant to be called from a single thread (the Ingestor's consumer).
/// stats getters may be called from any thread.
class SegmentWriter {
public:
    /// Creates the directory if needed and finalises any *.ipseg.part left
    /// behind by a crash (their intact batches stay readable).
    SegmentWriter(SegmentWriterOptions options, std::vector<TagInfo> tags);
    ~SegmentWriter();

    SegmentWriter(const SegmentWriter&) = delete;
    SegmentWriter& operator=(const SegmentWriter&) = delete;

    /// Appends one encoded batch, opening or rotating segments as needed.
    /// Each batch is flushed to the OS before returning. Throws
    /// std::runtime_error on an I/O error; the damaged segment is closed
    /// and the next batch goes to a fresh one.
    void write_batch(std::span<const std::uint8_t> batch);

    /// Finishes the current segment (renames it from .part). Idempotent.
    void close() noexcept;

    [[nodiscard]] std::uint64_t bytes_written() const noexcept {
        return bytes_written_.load(std::memory_order_relaxed);
    }
    [[nodiscard]] std::uint64_t segments_completed() const noexcept {
        return segments_completed_.load(std::memory_order_relaxed);
    }
    [[nodiscard]] std::size_t recovered_on_startup() const noexcept {
        return recovered_;
    }
    [[nodiscard]] const std::filesystem::path& directory() const noexcept {
        return options_.directory;
    }

private:
    void open_segment();
    void finish_segment() noexcept;

    SegmentWriterOptions options_;
    std::string tag_table_;
    std::ofstream out_;
    std::filesystem::path partial_path_;
    std::filesystem::path final_path_;
    std::uint64_t segment_bytes_ = 0;
    std::uint64_t sequence_ = 0;
    std::size_t recovered_ = 0;
    std::atomic<std::uint64_t> bytes_written_{0};
    std::atomic<std::uint64_t> segments_completed_{0};
};

/// Outcome of reading one segment.
struct SegmentReadResult {
    std::vector<TagInfo> tags;
    std::int64_t created_ms = 0;
    std::uint64_t batches = 0;          // batches that verified and were delivered
    std::uint64_t samples = 0;          // samples in those batches
    std::uint64_t corrupt_batches = 0;  // CRC failures or unrecognisable regions skipped
    bool truncated_tail = false;        // the file ends partway through a batch
    std::string error;                  // non-empty if the file is not a readable segment at all
};

/// Callback receiving one verified batch together with the segment's tag table.
using BatchVisitor = std::function<void(std::span<const TelemetrySample> samples, const std::vector<TagInfo>& tags)>;

/// Reads a segment, calling `on_batch` with the samples of every batch that
/// passes its CRC check. Damaged batches are skipped (their length is known
/// from the header); if a header itself is unreadable the reader scans
/// forward to the next batch boundary. A cut-off last batch, typical of a
/// crash mid-write, is reported via truncated_tail rather than as an error.
SegmentReadResult read_segment(const std::filesystem::path& path, const BatchVisitor& on_batch);

}  // namespace ironpulse::ingest
