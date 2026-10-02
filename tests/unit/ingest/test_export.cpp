#include <algorithm>
#include <atomic>
#include <catch2/catch_test_macros.hpp>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <set>
#include <string>
#include <thread>
#include <vector>

#include "ironpulse/core/config.hpp"
#include "ironpulse/ingest/reading_exporter.hpp"
#include "ironpulse/ingest/segment.hpp"

using namespace ironpulse::ingest;
namespace fs = std::filesystem;

namespace {

/// A fresh directory that is removed when the test ends.
class TempDir {
public:
    TempDir() {
        static std::atomic<int> counter{0};
        path_ = fs::temp_directory_path() /
                ("ironpulse_export_test_" +
                 std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + "_" +
                 std::to_string(counter++));
        fs::create_directories(path_);
    }
    ~TempDir() {
        std::error_code ec;
        fs::remove_all(path_, ec);
    }
    TempDir(const TempDir&) = delete;
    TempDir& operator=(const TempDir&) = delete;

    [[nodiscard]] const fs::path& path() const {
        return path_;
    }

private:
    fs::path path_;
};

std::vector<fs::path> files_with(const fs::path& dir, const std::string& ending) {
    std::vector<fs::path> found;
    for (const auto& entry : fs::directory_iterator(dir)) {
        const auto name = entry.path().filename().string();
        if (name.size() >= ending.size() && name.compare(name.size() - ending.size(), ending.size(), ending) == 0) {
            found.push_back(entry.path());
        }
    }
    std::sort(found.begin(), found.end());
    return found;
}

std::vector<fs::path> segments(const fs::path& dir) {
    return files_with(dir, kSegmentExtension);
}

std::vector<TagInfo> two_tags() {
    return {
        TagInfo{.id = 1, .sensor_id = "winding_temp", .device_id = "transformer_01", .name = "Winding", .unit = "°C"},
        TagInfo{.id = 2, .sensor_id = "vibration", .device_id = "pump_02", .name = "Vibration, \"RMS\"", .unit = "mm/s"},
    };
}

std::vector<std::uint8_t> encode(std::uint64_t first, std::uint32_t count) {
    std::vector<TelemetrySample> samples;
    for (std::uint32_t i = 0; i < count; ++i) {
        samples.push_back(TelemetrySample{
            .timestamp_ms = first + i, .value = static_cast<double>(first + i), .tag_id = 1 + i % 2, .quality = 0});
    }
    return Serializer::serialize(samples);
}

std::vector<TelemetrySample> read_all(const fs::path& file, SegmentReadResult* result_out = nullptr) {
    std::vector<TelemetrySample> all;
    auto result = read_segment(file, [&](std::span<const TelemetrySample> batch, const std::vector<TagInfo>&) {
        all.insert(all.end(), batch.begin(), batch.end());
    });
    if (result_out != nullptr) {
        *result_out = std::move(result);
    }
    return all;
}

ironpulse::core::ExportConfig export_config(const fs::path& dir) {
    ironpulse::core::ExportConfig cfg;
    cfg.enabled = true;
    cfg.directory = dir.string();
    cfg.queue_capacity = 4096;
    cfg.batch_size = 64;
    cfg.flush_interval_ms = 50;
    cfg.segment_max_mb = 64;
    return cfg;
}

}  // namespace

TEST_CASE("Tag tables survive an encode/decode round trip", "[ingest][segment]") {
    const auto decoded = decode_tag_table(encode_tag_table(two_tags()));
    REQUIRE(decoded.size() == 2);
    CHECK(decoded[1].id == 2);
    CHECK(decoded[1].sensor_id == "vibration");
    CHECK(decoded[1].name == "Vibration, \"RMS\"");
    CHECK(decoded[0].unit == "°C");
    CHECK_THROWS(decode_tag_table("{not json"));
}

TEST_CASE("SegmentWriter output reads back batch by batch", "[ingest][segment]") {
    TempDir dir;
    {
        SegmentWriter writer({.directory = dir.path()}, two_tags());
        writer.write_batch(encode(1000, 10));
        writer.write_batch(encode(2000, 5));
        CHECK(files_with(dir.path(), kPartialSuffix).size() == 1);  // in progress
    }
    CHECK(files_with(dir.path(), kPartialSuffix).empty());  // finalised on close
    const auto files = segments(dir.path());
    REQUIRE(files.size() == 1);

    SegmentReadResult result;
    const auto samples = read_all(files.front(), &result);
    CHECK(result.error.empty());
    CHECK(result.batches == 2);
    CHECK(result.samples == 15);
    CHECK(result.corrupt_batches == 0);
    CHECK_FALSE(result.truncated_tail);
    REQUIRE(result.tags.size() == 2);
    CHECK(result.tags[0].sensor_id == "winding_temp");
    REQUIRE(samples.size() == 15);
    CHECK(samples.front().timestamp_ms == 1000);
    CHECK(samples.back().timestamp_ms == 2004);
}

TEST_CASE("SegmentWriter does not create a file until there is something to write", "[ingest][segment]") {
    TempDir dir;
    { SegmentWriter writer({.directory = dir.path()}, two_tags()); }
    CHECK(fs::is_empty(dir.path()));
}

TEST_CASE("SegmentWriter starts a new segment when the size limit is reached", "[ingest][segment]") {
    TempDir dir;
    const auto batch = encode(0, 100);  // 16 + 2400 bytes
    {
        SegmentWriter writer({.directory = dir.path(), .max_segment_bytes = 6000}, two_tags());
        for (int i = 0; i < 5; ++i) {
            writer.write_batch(batch);
        }
        CHECK(writer.segments_completed() >= 2);
    }
    const auto files = segments(dir.path());
    REQUIRE(files.size() >= 3);
    std::uint64_t total = 0;
    for (const auto& file : files) {
        CHECK(fs::file_size(file) <= 6000);
        SegmentReadResult result;
        read_all(file, &result);
        CHECK(result.error.empty());
        CHECK(result.tags.size() == 2);  // every segment is self-describing
        total += result.samples;
    }
    CHECK(total == 500);
}

TEST_CASE("read_segment skips a damaged batch and keeps the rest", "[ingest][segment]") {
    TempDir dir;
    {
        SegmentWriter writer({.directory = dir.path()}, two_tags());
        writer.write_batch(encode(0, 4));
        writer.write_batch(encode(100, 4));
        writer.write_batch(encode(200, 4));
    }
    const auto file = segments(dir.path()).front();
    {
        // Flip one byte inside the samples of the second batch.
        const auto size = fs::file_size(file);
        const auto batch_bytes = Serializer::required_buffer_size(4);
        std::fstream f(file, std::ios::in | std::ios::out | std::ios::binary);
        f.seekp(static_cast<std::streamoff>(size - 2 * batch_bytes + sizeof(BatchHeader) + 5));
        f.put('\x7F');
    }
    SegmentReadResult result;
    const auto samples = read_all(file, &result);
    CHECK(result.batches == 2);
    CHECK(result.corrupt_batches == 1);
    REQUIRE(samples.size() == 8);
    CHECK(samples[4].timestamp_ms == 200);
}

TEST_CASE("read_segment resynchronises after an unreadable batch header", "[ingest][segment]") {
    TempDir dir;
    {
        SegmentWriter writer({.directory = dir.path()}, two_tags());
        writer.write_batch(encode(0, 4));
        writer.write_batch(encode(100, 4));
        writer.write_batch(encode(200, 4));
    }
    const auto file = segments(dir.path()).front();
    {
        const auto size = fs::file_size(file);
        const auto batch_bytes = Serializer::required_buffer_size(4);
        std::fstream f(file, std::ios::in | std::ios::out | std::ios::binary);
        f.seekp(static_cast<std::streamoff>(size - 2 * batch_bytes));  // magic of the second batch
        f.write("XXXX", 4);
    }
    SegmentReadResult result;
    const auto samples = read_all(file, &result);
    CHECK(result.corrupt_batches == 1);
    CHECK(result.batches == 2);
    REQUIRE(samples.size() == 8);
    CHECK(samples.back().timestamp_ms == 203);
}

TEST_CASE("read_segment reports a cut-off last batch without failing", "[ingest][segment]") {
    TempDir dir;
    {
        SegmentWriter writer({.directory = dir.path()}, two_tags());
        writer.write_batch(encode(0, 4));
        writer.write_batch(encode(100, 4));
    }
    const auto file = segments(dir.path()).front();
    fs::resize_file(file, fs::file_size(file) - 10);

    SegmentReadResult result;
    const auto samples = read_all(file, &result);
    CHECK(result.error.empty());
    CHECK(result.truncated_tail);
    CHECK(result.batches == 1);
    CHECK(samples.size() == 4);
}

TEST_CASE("read_segment rejects files that are not segments", "[ingest][segment]") {
    TempDir dir;
    const auto file = dir.path() / "notes.ipseg";
    std::ofstream(file) << "this is not a segment, just some text that is long enough";
    CHECK_FALSE(read_segment(file, nullptr).error.empty());
    CHECK_FALSE(read_segment(dir.path() / "missing.ipseg", nullptr).error.empty());
}

TEST_CASE("SegmentWriter finalises segments left unfinished by a crash", "[ingest][segment]") {
    TempDir dir;
    {
        SegmentWriter writer({.directory = dir.path()}, two_tags());
        writer.write_batch(encode(0, 3));
        // Simulate a crash: copy the in-progress file before close() renames it.
        const auto partial = files_with(dir.path(), kPartialSuffix);
        REQUIRE(partial.size() == 1);
        fs::copy_file(partial.front(), dir.path() / "telemetry-crashed.ipseg.part");
    }
    SegmentWriter restarted({.directory = dir.path()}, two_tags());
    CHECK(restarted.recovered_on_startup() == 1);
    CHECK(files_with(dir.path(), kPartialSuffix).empty());
    SegmentReadResult result;
    read_all(dir.path() / "telemetry-crashed.ipseg", &result);
    CHECK(result.samples == 3);
}

TEST_CASE("tags_from_devices numbers sensors across devices in config order", "[ingest][exporter]") {
    ironpulse::core::ModbusDeviceConfig a;
    a.id = "dev_a";
    a.sensors.resize(2);
    a.sensors[0].id = "s1";
    a.sensors[1].id = "s2";
    ironpulse::core::ModbusDeviceConfig b;
    b.id = "dev_b";
    b.sensors.resize(1);
    b.sensors[0].id = "s3";
    b.sensors[0].unit = "bar";

    const auto tags = tags_from_devices({a, b});
    REQUIRE(tags.size() == 3);
    CHECK(tags[0].id == 1);
    CHECK(tags[2].id == 3);
    CHECK(tags[2].sensor_id == "s3");
    CHECK(tags[2].device_id == "dev_b");
    CHECK(tags[2].unit == "bar");
}

TEST_CASE("ReadingExporter writes every reading from many threads exactly once", "[ingest][exporter][concurrency]") {
    TempDir dir;
    constexpr int kThreads = 4;
    constexpr int kPerThread = 5000;
    const auto start = std::chrono::system_clock::time_point(std::chrono::milliseconds(1'700'000'000'000));
    {
        ReadingExporter exporter(export_config(dir.path()), two_tags());
        exporter.start();
        std::vector<std::thread> producers;
        for (int t = 0; t < kThreads; ++t) {
            producers.emplace_back([&, t] {
                const std::string sensor = t % 2 == 0 ? "winding_temp" : "vibration";
                for (int i = 0; i < kPerThread; ++i) {
                    // Encode (thread, index) into the value so duplicates and gaps show up.
                    while (!exporter.submit(sensor, t * 1'000'000.0 + i, start + std::chrono::milliseconds(i))) {
                        std::this_thread::yield();  // queue full: wait for the writer
                    }
                }
            });
        }
        for (auto& p : producers) {
            p.join();
        }
        exporter.stop();
        const auto stats = exporter.stats();
        CHECK(stats.accepted == kThreads * kPerThread);
        CHECK(stats.batches_failed == 0);
        CHECK(stats.bytes_written > 0);
    }

    std::multiset<double> values;
    for (const auto& file : segments(dir.path())) {
        for (const auto& s : read_all(file)) {
            values.insert(s.value);
            CHECK((s.tag_id == 1 || s.tag_id == 2));
        }
    }
    REQUIRE(values.size() == static_cast<std::size_t>(kThreads * kPerThread));
    bool all_once = true;
    for (int t = 0; t < kThreads; ++t) {
        for (int i = 0; i < kPerThread; i += 499) {
            all_once = all_once && values.count(t * 1'000'000.0 + i) == 1;
        }
    }
    CHECK(all_once);
}

TEST_CASE("ReadingExporter ignores unknown sensors and readings after stop", "[ingest][exporter]") {
    TempDir dir;
    ReadingExporter exporter(export_config(dir.path()), two_tags());
    const auto now = std::chrono::system_clock::now();
    CHECK_FALSE(exporter.submit("winding_temp", 1.0, now));  // not started yet
    exporter.start();
    CHECK(exporter.submit("winding_temp", 1.0, now));
    CHECK_FALSE(exporter.submit("no_such_sensor", 1.0, now));
    exporter.stop();
    CHECK_FALSE(exporter.submit("winding_temp", 2.0, now));

    const auto stats = exporter.stats();
    CHECK(stats.accepted == 1);
    CHECK(stats.unknown_sensor == 1);
    REQUIRE(segments(dir.path()).size() == 1);
    const auto samples = read_all(segments(dir.path()).front());
    REQUIRE(samples.size() == 1);
    CHECK(samples[0].tag_id == 1);
    CHECK(samples[0].timestamp_ms ==
          static_cast<std::uint64_t>(
              std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count()));
}
