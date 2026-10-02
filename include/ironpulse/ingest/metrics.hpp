#pragma once

#include <atomic>
#include <cstdint>

#include "ironpulse/ingest/cache_line.hpp"

namespace ironpulse::ingest {

/// Point-in-time copy of an Ingestor's counters.
struct CountersSnapshot {
    std::uint64_t ingested = 0;        // samples accepted into the queue
    std::uint64_t dropped = 0;         // samples rejected because the queue was full
    std::uint64_t batches_sent = 0;    // batches handed to the sink successfully
    std::uint64_t batches_failed = 0;  // batches whose sink threw
};

/// Lock-free counters for the ingest hot path. Each counter sits on its
/// own cache line, because the producer increments `ingested`/`dropped`
/// while the consumer increments the batch counters, and sharing a line
/// would make every increment bounce it between cores. Relaxed ordering is
/// enough: the values are statistics, not synchronisation.
class Counters {
public:
    void record_ingested() noexcept {
        ingested_.fetch_add(1, std::memory_order_relaxed);
    }
    void record_dropped() noexcept {
        dropped_.fetch_add(1, std::memory_order_relaxed);
    }
    void record_batch_sent() noexcept {
        batches_sent_.fetch_add(1, std::memory_order_relaxed);
    }
    void record_batch_failed() noexcept {
        batches_failed_.fetch_add(1, std::memory_order_relaxed);
    }

    [[nodiscard]] CountersSnapshot snapshot() const noexcept {
        return CountersSnapshot{
            .ingested = ingested_.load(std::memory_order_relaxed),
            .dropped = dropped_.load(std::memory_order_relaxed),
            .batches_sent = batches_sent_.load(std::memory_order_relaxed),
            .batches_failed = batches_failed_.load(std::memory_order_relaxed),
        };
    }

private:
    alignas(kCacheLineSize) std::atomic<std::uint64_t> ingested_{0};
    alignas(kCacheLineSize) std::atomic<std::uint64_t> dropped_{0};
    alignas(kCacheLineSize) std::atomic<std::uint64_t> batches_sent_{0};
    alignas(kCacheLineSize) std::atomic<std::uint64_t> batches_failed_{0};
};

}  // namespace ironpulse::ingest
