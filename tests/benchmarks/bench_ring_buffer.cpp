#include <benchmark/benchmark.h>

#include <thread>
#include <vector>

#include "ironpulse/storage/ring_buffer.hpp"

using ironpulse::storage::RingBuffer;

// Single-threaded push — establishes the baseline cost of the mutex-guarded
// write path with no contention (see docs/adr/0002-storage-format.md for
// why a plain mutex was chosen over a lock-free structure; this benchmark
// is what would justify revisiting that decision).
static void BM_RingBufferPush(benchmark::State& state) {
    RingBuffer<double> buffer(4096);
    double value = 0.0;
    for (auto _ : state) {
        buffer.push(value);
        value += 1.0;
    }
}
BENCHMARK(BM_RingBufferPush);

// Concurrent push from N threads, simulating N Modbus devices being polled
// in parallel and all landing in independent buffers' shared code path.
static void BM_RingBufferConcurrentPush(benchmark::State& state) {
    static RingBuffer<double> buffer(4096);
    for (auto _ : state) {
        buffer.push(1.0);
    }
}
BENCHMARK(BM_RingBufferConcurrentPush)->Threads(1)->Threads(2)->Threads(4)->Threads(8);

// recent() under a steady write load — this is the path the REST API's
// GET /api/v1/series/{id} takes, and the path RuleEngine reads indirectly
// via SeriesStore.
static void BM_RingBufferRecent(benchmark::State& state) {
    RingBuffer<double> buffer(4096);
    for (int i = 0; i < 4096; ++i) {
        buffer.push(static_cast<double>(i));
    }
    const auto count = static_cast<std::size_t>(state.range(0));
    for (auto _ : state) {
        auto samples = buffer.recent(count);
        benchmark::DoNotOptimize(samples);
    }
}
BENCHMARK(BM_RingBufferRecent)->Arg(10)->Arg(100)->Arg(1000)->Arg(4096);
