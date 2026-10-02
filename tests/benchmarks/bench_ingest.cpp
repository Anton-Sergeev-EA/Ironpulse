#include <benchmark/benchmark.h>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <thread>
#include <vector>

#include "ironpulse/ingest/ingestor.hpp"
#include "ironpulse/ingest/reading_exporter.hpp"
#include "ironpulse/ingest/ring_buffer.hpp"
#include "ironpulse/ingest/serializer.hpp"

using namespace ironpulse::ingest;

// Uncontended push + pop on one thread: the floor cost of the SPSC queue,
// to compare with BM_RingBufferPush (the storage layer's mutex buffer).
static void BM_SpscPushPop(benchmark::State& state) {
    SpscRingBuffer<TelemetrySample> queue(4096);
    TelemetrySample sample{.timestamp_ms = 1, .value = 1.0, .tag_id = 1, .quality = 0};
    TelemetrySample out{};
    for (auto _ : state) {
        queue.push(sample);
        queue.pop(out);
        benchmark::DoNotOptimize(out);
    }
}
BENCHMARK(BM_SpscPushPop);

// Producer and consumer on different cores: what the cached-index and
// cache-line-separation design is for. Reports items per second.
static void BM_SpscTwoThreadThroughput(benchmark::State& state) {
    constexpr std::int64_t kItems = 1 << 20;
    for (auto _ : state) {
        SpscRingBuffer<std::uint64_t> queue(1 << 14);
        std::thread consumer([&] {
            std::uint64_t value = 0;
            for (std::int64_t received = 0; received < kItems;) {
                if (queue.pop(value)) {
                    ++received;
                }
            }
            benchmark::DoNotOptimize(value);
        });
        for (std::uint64_t i = 0; i < static_cast<std::uint64_t>(kItems); ++i) {
            while (!queue.push(i)) {
            }
        }
        consumer.join();
    }
    state.SetItemsProcessed(state.iterations() * kItems);
}
BENCHMARK(BM_SpscTwoThreadThroughput)->UseRealTime()->Unit(benchmark::kMillisecond);

// Encoding a full 1000-sample batch, CRC included. Reports bytes per second.
static void BM_SerializeBatch(benchmark::State& state) {
    std::vector<TelemetrySample> samples(static_cast<std::size_t>(state.range(0)));
    for (std::size_t i = 0; i < samples.size(); ++i) {
        samples[i] = TelemetrySample{
            .timestamp_ms = i, .value = static_cast<double>(i), .tag_id = static_cast<std::uint32_t>(i % 16)};
    }
    std::vector<std::uint8_t> out(Serializer::required_buffer_size(samples.size()));
    for (auto _ : state) {
        benchmark::DoNotOptimize(Serializer::serialize(samples, out));
    }
    state.SetBytesProcessed(state.iterations() * static_cast<std::int64_t>(out.size()));
    state.SetLabel(Serializer::hardware_crc_available() ? "crc32c: sse4.2" : "crc32c: software");
}
BENCHMARK(BM_SerializeBatch)->Arg(1000);

static void BM_Crc32cSoftware(benchmark::State& state) {
    std::vector<std::uint8_t> data(static_cast<std::size_t>(state.range(0)), 0xA5);
    for (auto _ : state) {
        benchmark::DoNotOptimize(Serializer::crc32c_software(data.data(), data.size()));
    }
    state.SetBytesProcessed(state.iterations() * state.range(0));
}
BENCHMARK(BM_Crc32cSoftware)->Arg(24000);

// What a polling strand pays per reading with export enabled: tag lookup,
// the producer critical section and the queue push, with N strands at once.
static void BM_ExporterSubmit(benchmark::State& state) {
    static ReadingExporter* exporter = nullptr;
    static std::filesystem::path dir;
    if (state.thread_index() == 0) {
        dir = std::filesystem::temp_directory_path() / "ironpulse_bench_export";
        std::filesystem::remove_all(dir);
        ironpulse::core::ExportConfig cfg;
        cfg.directory = dir.string();
        cfg.queue_capacity = 1 << 20;
        cfg.batch_size = 4096;
        exporter = new ReadingExporter(
            cfg, {TagInfo{.id = 1, .sensor_id = "winding_temp"}, TagInfo{.id = 2, .sensor_id = "vibration"}});
        exporter->start();
    }
    const auto now = std::chrono::system_clock::now();
    for (auto _ : state) {
        benchmark::DoNotOptimize(exporter->submit("vibration", 1.0, now));
    }
    if (state.thread_index() == 0) {
        exporter->stop();
        delete exporter;
        std::filesystem::remove_all(dir);
    }
}
BENCHMARK(BM_ExporterSubmit)->Threads(1)->Threads(4);
