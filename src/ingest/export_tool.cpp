// ironpulse-export — inspect and convert Ironpulse export segments.
//
//   ironpulse-export verify <segment|directory>...
//   ironpulse-export tags   <segment>
//   ironpulse-export dump   [--sensor ID] <segment|directory>...   (CSV on stdout)

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <ctime>
#include <filesystem>
#include <iostream>
#include <map>
#include <string>
#include <vector>

#include "ironpulse/ingest/segment.hpp"
#include "ironpulse/version.hpp"

namespace fs = std::filesystem;
using namespace ironpulse::ingest;

namespace {

int usage() {
    std::cerr << "ironpulse-export " << ironpulse::kVersion << "\n\n"
              << "Usage:\n"
              << "  ironpulse-export verify <segment|directory>...\n"
              << "      Check every batch's CRC; exit status 1 if anything is damaged.\n"
              << "  ironpulse-export tags <segment>\n"
              << "      Print the segment's tag table (tag id -> sensor).\n"
              << "  ironpulse-export dump [--sensor ID] <segment|directory>...\n"
              << "      Write readings as CSV to stdout:\n"
              << "      timestamp,sensor_id,device_id,value,unit,quality\n\n"
              << "A directory stands for all finished *.ipseg files in it, oldest first.\n";
    return 2;
}

/// Expands directories into their finished segments, sorted by name
/// (names start with the creation time, so this is chronological).
std::vector<fs::path> expand(const std::vector<std::string>& args) {
    std::vector<fs::path> files;
    for (const auto& arg : args) {
        const fs::path path(arg);
        if (fs::is_directory(path)) {
            std::vector<fs::path> found;
            for (const auto& entry : fs::directory_iterator(path)) {
                if (entry.is_regular_file() && entry.path().extension() == kSegmentExtension) {
                    found.push_back(entry.path());
                }
            }
            std::sort(found.begin(), found.end());
            files.insert(files.end(), found.begin(), found.end());
        } else {
            files.push_back(path);
        }
    }
    return files;
}

std::string iso8601(std::uint64_t epoch_ms) {
    const auto seconds = static_cast<std::time_t>(epoch_ms / 1000);
    std::tm utc{};
#if defined(_WIN32)
    gmtime_s(&utc, &seconds);
#else
    gmtime_r(&seconds, &utc);
#endif
    char stamp[32];
    std::strftime(stamp, sizeof(stamp), "%Y-%m-%dT%H:%M:%S", &utc);
    char out[48];
    std::snprintf(out, sizeof(out), "%s.%03uZ", stamp, static_cast<unsigned>(epoch_ms % 1000));
    return out;
}

/// CSV field quoting per RFC 4180.
std::string csv(const std::string& field) {
    if (field.find_first_of(",\"\n") == std::string::npos) {
        return field;
    }
    std::string quoted = "\"";
    for (const char c : field) {
        quoted += c;
        if (c == '"') {
            quoted += '"';
        }
    }
    return quoted + "\"";
}

int verify(const std::vector<fs::path>& files) {
    bool all_ok = true;
    for (const auto& file : files) {
        const auto result = read_segment(file, nullptr);
        if (!result.error.empty()) {
            std::cout << file.string() << ": ERROR " << result.error << "\n";
            all_ok = false;
            continue;
        }
        const bool ok = result.corrupt_batches == 0 && !result.truncated_tail;
        std::cout << file.string() << ": " << (ok ? "OK" : "DAMAGED") << " — " << result.batches
                  << " batch(es), " << result.samples << " reading(s), " << result.tags.size() << " tag(s)";
        if (result.corrupt_batches > 0) {
            std::cout << ", " << result.corrupt_batches << " damaged batch(es) skipped";
        }
        if (result.truncated_tail) {
            std::cout << ", last batch cut off";
        }
        std::cout << "\n";
        all_ok = all_ok && ok;
    }
    return all_ok ? 0 : 1;
}

int tags(const fs::path& file) {
    const auto result = read_segment(file, nullptr);
    if (!result.error.empty()) {
        std::cerr << file.string() << ": " << result.error << "\n";
        return 1;
    }
    std::cout << "tag_id,sensor_id,device_id,name,unit\n";
    for (const auto& tag : result.tags) {
        std::cout << tag.id << ',' << csv(tag.sensor_id) << ',' << csv(tag.device_id) << ',' << csv(tag.name)
                  << ',' << csv(tag.unit) << '\n';
    }
    return 0;
}

int dump(const std::vector<fs::path>& files, const std::string& sensor_filter) {
    int status = 0;
    std::cout << "timestamp,sensor_id,device_id,value,unit,quality\n";
    for (const auto& file : files) {
        std::map<std::uint32_t, const TagInfo*> by_id;
        const auto result = read_segment(file, [&](std::span<const TelemetrySample> batch, const std::vector<TagInfo>& tags) {
            if (by_id.empty()) {
                for (const auto& tag : tags) {
                    by_id[tag.id] = &tag;
                }
            }
            for (const auto& sample : batch) {
                const auto it = by_id.find(sample.tag_id);
                const TagInfo* tag = it != by_id.end() ? it->second : nullptr;
                const std::string sensor = tag != nullptr ? tag->sensor_id : "tag_" + std::to_string(sample.tag_id);
                if (!sensor_filter.empty() && sensor != sensor_filter) {
                    continue;
                }
                char value[32];
                std::snprintf(value, sizeof(value), "%.17g", sample.value);
                std::cout << iso8601(sample.timestamp_ms) << ',' << csv(sensor) << ','
                          << csv(tag != nullptr ? tag->device_id : "") << ',' << value << ','
                          << csv(tag != nullptr ? tag->unit : "") << ',' << static_cast<unsigned>(sample.quality)
                          << '\n';
            }
        });
        if (!result.error.empty()) {
            std::cerr << file.string() << ": " << result.error << "\n";
            status = 1;
        } else if (result.corrupt_batches > 0 || result.truncated_tail) {
            std::cerr << file.string() << ": " << result.corrupt_batches << " damaged batch(es) skipped"
                      << (result.truncated_tail ? ", last batch cut off" : "") << "\n";
        }
    }
    return status;
}

}  // namespace

int main(int argc, char** argv) {
    if (argc < 3) {
        return usage();
    }
    const std::string command = argv[1];
    std::string sensor_filter;
    std::vector<std::string> args;
    for (int i = 2; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--sensor" && i + 1 < argc) {
            sensor_filter = argv[++i];
        } else {
            args.push_back(arg);
        }
    }
    if (args.empty()) {
        return usage();
    }

    try {
        if (command == "verify") {
            return verify(expand(args));
        }
        if (command == "tags") {
            return tags(args.front());
        }
        if (command == "dump") {
            return dump(expand(args), sensor_filter);
        }
    } catch (const std::exception& e) {
        std::cerr << "ironpulse-export: " << e.what() << "\n";
        return 1;
    }
    return usage();
}
