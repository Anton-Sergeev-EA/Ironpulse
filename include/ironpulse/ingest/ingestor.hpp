#pragma once

#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <span>
#include <thread>
#include <vector>

#include "ironpulse/ingest/metrics.hpp"
#include "ironpulse/ingest/ring_buffer.hpp"
#include "ironpulse/ingest/serializer.hpp"

namespace ironpulse::ingest {

struct IngestorOptions {
    /// Queue length in samples; a power of two. Samples arriving while the
    /// queue is full are dropped and counted, never blocking the producer.
    std::size_t queue_capacity = 65536;
    /// A batch is sent as soon as it holds this many samples...
    std::size_t batch_size = 1000;
    /// ...or when this long has passed since the last one, whichever is first.
    std::chrono::milliseconds flush_interval{1000};
};

/// Moves samples from a producer thread to a sink without ever blocking
/// the producer: the producer enqueues into a lock-free SPSC queue; a
/// dedicated consumer thread drains it, groups samples into batches,
/// encodes each batch with Serializer into a reusable buffer and hands the
/// bytes to the sink. After warm-up the consumer allocates nothing.
///
/// Single producer: `submit` must not be called from two threads at once.
/// The sink runs on the consumer thread; if it throws, the batch is
/// counted as failed and the consumer carries on with the next one.
class Ingestor {
public:
    using BatchSink = std::function<void(std::span<const std::uint8_t> batch)>;

    /// Throws std::invalid_argument for invalid options.
    Ingestor(IngestorOptions options, BatchSink sink);
    ~Ingestor();

    Ingestor(const Ingestor&) = delete;
    Ingestor& operator=(const Ingestor&) = delete;
    Ingestor(Ingestor&&) = delete;
    Ingestor& operator=(Ingestor&&) = delete;

    /// Starts the consumer thread. Idempotent.
    void start();

    /// Stops accepting work, drains everything already queued to the sink
    /// and joins the consumer thread. Idempotent. The producer must have
    /// stopped calling submit() before this is called.
    void stop() noexcept;

    /// Enqueues one sample (producer thread only). Returns false, counting
    /// a drop, when the queue is full.
    bool submit(const TelemetrySample& sample) noexcept;

    [[nodiscard]] CountersSnapshot counters() const noexcept {
        return counters_.snapshot();
    }

    /// Samples currently waiting in the queue (approximate while running).
    [[nodiscard]] std::size_t queue_depth() const noexcept {
        return queue_.size();
    }

    [[nodiscard]] bool running() const noexcept {
        return running_.load(std::memory_order_acquire);
    }

    [[nodiscard]] const IngestorOptions& options() const noexcept {
        return options_;
    }

private:
    void consumer_loop(const std::stop_token& stop);
    void send_batch(std::vector<TelemetrySample>& batch, std::vector<std::uint8_t>& encoded) noexcept;

    IngestorOptions options_;
    BatchSink sink_;
    SpscRingBuffer<TelemetrySample> queue_;
    Counters counters_;
    std::atomic<bool> running_{false};
    std::jthread consumer_;
};

}  // namespace ironpulse::ingest
