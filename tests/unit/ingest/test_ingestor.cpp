#include <atomic>
#include <catch2/catch_test_macros.hpp>
#include <chrono>
#include <mutex>
#include <stdexcept>
#include <thread>
#include <vector>

#include "ironpulse/ingest/ingestor.hpp"

using namespace ironpulse::ingest;
using namespace std::chrono_literals;

namespace {

/// Collects decoded batches from the Ingestor's thread. Catch2 assertions
/// are not thread-safe, so a decoding failure is only recorded here and
/// checked by the test thread (see the destructor).
struct Collector {
    std::mutex mutex;
    std::vector<std::vector<TelemetrySample>> batches;
    std::atomic<int> undecodable{0};

    ~Collector() {
        CHECK(undecodable.load() == 0);
    }

    Ingestor::BatchSink sink() {
        return [this](std::span<const std::uint8_t> bytes) {
            std::vector<TelemetrySample> samples;
            if (Serializer::decode_copy(bytes, samples) != DecodeStatus::ok) {
                undecodable.fetch_add(1);
            }
            std::lock_guard lock(mutex);
            batches.push_back(std::move(samples));
        };
    }

    std::size_t total() {
        std::lock_guard lock(mutex);
        std::size_t n = 0;
        for (const auto& b : batches) {
            n += b.size();
        }
        return n;
    }
};

TelemetrySample sample(std::uint64_t i) {
    return TelemetrySample{.timestamp_ms = i, .value = static_cast<double>(i), .tag_id = 1, .quality = 0};
}

template <typename Pred>
bool wait_until(Pred pred, std::chrono::milliseconds timeout = 2000ms) {
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    while (!pred()) {
        if (std::chrono::steady_clock::now() > deadline) {
            return false;
        }
        std::this_thread::sleep_for(1ms);
    }
    return true;
}

}  // namespace

TEST_CASE("Ingestor rejects invalid options", "[ingest][ingestor]") {
    auto sink = [](std::span<const std::uint8_t>) {};
    CHECK_THROWS_AS(Ingestor(IngestorOptions{.queue_capacity = 100}, sink), std::invalid_argument);
    CHECK_THROWS_AS(Ingestor(IngestorOptions{.queue_capacity = 64, .batch_size = 0}, sink),
                    std::invalid_argument);
    CHECK_THROWS_AS(Ingestor(IngestorOptions{.queue_capacity = 64, .batch_size = 65}, sink),
                    std::invalid_argument);
    CHECK_THROWS_AS(
        Ingestor(IngestorOptions{.queue_capacity = 64, .batch_size = 8, .flush_interval = 0ms}, sink),
        std::invalid_argument);
    CHECK_THROWS_AS(Ingestor(IngestorOptions{.queue_capacity = 64, .batch_size = 8}, nullptr),
                    std::invalid_argument);
}

TEST_CASE("Ingestor sends a batch as soon as it is full", "[ingest][ingestor]") {
    Collector collector;
    Ingestor ingestor({.queue_capacity = 64, .batch_size = 4, .flush_interval = 1h}, collector.sink());
    ingestor.start();
    for (std::uint64_t i = 0; i < 8; ++i) {
        REQUIRE(ingestor.submit(sample(i)));
    }
    REQUIRE(wait_until([&] { return collector.total() == 8; }));
    ingestor.stop();

    REQUIRE(collector.batches.size() == 2);
    CHECK(collector.batches[0].size() == 4);
    CHECK(collector.batches[1].front().timestamp_ms == 4);
    CHECK(ingestor.counters().batches_sent == 2);
}

TEST_CASE("Ingestor flushes a partial batch when the interval expires", "[ingest][ingestor]") {
    Collector collector;
    Ingestor ingestor({.queue_capacity = 64, .batch_size = 32, .flush_interval = 20ms}, collector.sink());
    ingestor.start();
    REQUIRE(ingestor.submit(sample(1)));
    REQUIRE(ingestor.submit(sample(2)));
    CHECK(wait_until([&] { return collector.total() == 2; }));
    ingestor.stop();
}

TEST_CASE("Ingestor writes out everything still queued when stopped", "[ingest][ingestor]") {
    Collector collector;
    Ingestor ingestor({.queue_capacity = 1024, .batch_size = 100, .flush_interval = 1h}, collector.sink());
    ingestor.start();
    for (std::uint64_t i = 0; i < 250; ++i) {
        REQUIRE(ingestor.submit(sample(i)));
    }
    ingestor.stop();  // must not lose the last, partial batch

    CHECK(collector.total() == 250);
    CHECK(ingestor.counters().ingested == 250);
    CHECK_FALSE(ingestor.running());
}

TEST_CASE("Ingestor drops and counts samples when the queue is full", "[ingest][ingestor]") {
    Collector collector;
    Ingestor ingestor({.queue_capacity = 8, .batch_size = 8, .flush_interval = 1h}, collector.sink());
    // Not started: nothing drains the queue.
    std::size_t accepted = 0;
    for (std::uint64_t i = 0; i < 20; ++i) {
        accepted += ingestor.submit(sample(i)) ? 1U : 0U;
    }
    CHECK(accepted == 8);
    const auto counters = ingestor.counters();
    CHECK(counters.ingested == 8);
    CHECK(counters.dropped == 12);
    CHECK(ingestor.queue_depth() == 8);

    ingestor.start();
    ingestor.stop();
    CHECK(collector.total() == 8);
}

TEST_CASE("Ingestor keeps running when the sink throws", "[ingest][ingestor]") {
    std::atomic<int> calls{0};
    Ingestor ingestor({.queue_capacity = 64, .batch_size = 1, .flush_interval = 1h},
                      [&](std::span<const std::uint8_t>) {
                          if (calls.fetch_add(1) == 0) {
                              throw std::runtime_error("disk full");
                          }
                      });
    ingestor.start();
    REQUIRE(ingestor.submit(sample(1)));
    REQUIRE(ingestor.submit(sample(2)));
    REQUIRE(wait_until([&] { return calls.load() == 2; }));
    ingestor.stop();

    const auto counters = ingestor.counters();
    CHECK(counters.batches_failed == 1);
    CHECK(counters.batches_sent == 1);
}

TEST_CASE("Ingestor start and stop are idempotent", "[ingest][ingestor]") {
    Collector collector;
    Ingestor ingestor({.queue_capacity = 16, .batch_size = 4, .flush_interval = 1h}, collector.sink());
    ingestor.stop();
    ingestor.start();
    ingestor.start();
    CHECK(ingestor.running());
    ingestor.stop();
    ingestor.stop();
    CHECK_FALSE(ingestor.running());
}
