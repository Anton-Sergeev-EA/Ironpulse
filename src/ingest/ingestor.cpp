#include "ironpulse/ingest/ingestor.hpp"

#include <exception>
#include <stdexcept>
#include <utility>

#include "ironpulse/core/logger.hpp"

#if defined(__x86_64__) || defined(_M_X64)
#include <immintrin.h>  // _mm_pause
#endif

namespace ironpulse::ingest {

namespace {

/// Tells the core we are spinning, so a hyper-threaded sibling gets the
/// execution resources and the spin itself burns less power.
inline void cpu_relax() noexcept {
#if defined(__x86_64__) || defined(_M_X64)
    _mm_pause();
#elif defined(__aarch64__)
    asm volatile("yield" ::: "memory");
#else
    std::this_thread::yield();
#endif
}

/// Escalating back-off for an idle consumer: spin briefly (a sample may be
/// microseconds away under load), then yield, then sleep. Sleeping 1 ms at
/// most keeps an idle engine near 0% CPU while a burst still has to fill
/// the whole queue within that millisecond to cause a drop.
void back_off(unsigned idle_rounds) noexcept {
    constexpr unsigned kSpinRounds = 64;
    constexpr unsigned kYieldRounds = 128;
    if (idle_rounds < kSpinRounds) {
        cpu_relax();
    } else if (idle_rounds < kYieldRounds) {
        std::this_thread::yield();
    } else {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}

void validate(const IngestorOptions& options) {
    const auto capacity = options.queue_capacity;
    if (capacity < 2 || (capacity & (capacity - 1)) != 0) {
        throw std::invalid_argument("ingest: queue_capacity must be a power of two and at least 2");
    }
    if (options.batch_size == 0 || options.batch_size > capacity) {
        throw std::invalid_argument("ingest: batch_size must be between 1 and queue_capacity");
    }
    if (options.flush_interval.count() <= 0) {
        throw std::invalid_argument("ingest: flush_interval must be positive");
    }
}

}  // namespace

Ingestor::Ingestor(IngestorOptions options, BatchSink sink)
    : options_((validate(options), options)), sink_(std::move(sink)), queue_(options_.queue_capacity) {
    if (!sink_) {
        throw std::invalid_argument("ingest: a batch sink is required");
    }
}

Ingestor::~Ingestor() {
    stop();
}

void Ingestor::start() {
    if (running_.exchange(true, std::memory_order_acq_rel)) {
        return;
    }
    consumer_ = std::jthread([this](const std::stop_token& stop) { consumer_loop(stop); });
}

void Ingestor::stop() noexcept {
    if (!running_.exchange(false, std::memory_order_acq_rel)) {
        return;
    }
    consumer_.request_stop();
    if (consumer_.joinable()) {
        consumer_.join();
    }
}

bool Ingestor::submit(const TelemetrySample& sample) noexcept {
    if (queue_.push(sample)) {
        counters_.record_ingested();
        return true;
    }
    counters_.record_dropped();
    return false;
}

void Ingestor::send_batch(std::vector<TelemetrySample>& batch, std::vector<std::uint8_t>& encoded) noexcept {
    const std::size_t bytes = Serializer::serialize(batch, encoded);
    batch.clear();
    try {
        sink_(std::span<const std::uint8_t>(encoded.data(), bytes));
        counters_.record_batch_sent();
    } catch (const std::exception& e) {
        counters_.record_batch_failed();
        IP_LOG_ERROR("ingest: batch sink failed: {}", e.what());
    } catch (...) {
        counters_.record_batch_failed();
        IP_LOG_ERROR("ingest: batch sink failed with an unknown exception");
    }
}

void Ingestor::consumer_loop(const std::stop_token& stop) {
    // Both buffers are sized once, up front, for the largest possible batch,
    // so the steady-state loop never allocates.
    std::vector<TelemetrySample> batch;
    batch.reserve(options_.batch_size);
    std::vector<std::uint8_t> encoded(Serializer::required_buffer_size(options_.batch_size));

    auto last_flush = std::chrono::steady_clock::now();
    unsigned idle_rounds = 0;

    while (true) {
        // Read the stop flag *before* draining: everything the producer
        // queued before stop() was called is then guaranteed to be seen by
        // the drain below, and the final "queue empty" check is exact.
        const bool stopping = stop.stop_requested();

        std::size_t popped = 0;
        TelemetrySample sample{};
        while (batch.size() < options_.batch_size && queue_.pop(sample)) {
            batch.push_back(sample);
            ++popped;
        }

        const auto now = std::chrono::steady_clock::now();
        const bool full = batch.size() >= options_.batch_size;
        const bool due = !batch.empty() && now - last_flush >= options_.flush_interval;
        if (full || due) {
            send_batch(batch, encoded);
            last_flush = now;
            idle_rounds = 0;
            continue;
        }

        if (stopping) {
            if (queue_.empty()) {
                if (!batch.empty()) {
                    send_batch(batch, encoded);
                }
                return;
            }
            continue;  // keep draining
        }

        if (popped == 0) {
            back_off(idle_rounds);
            if (idle_rounds < 1024) {  // saturate instead of wrapping back to spinning
                ++idle_rounds;
            }
        } else {
            idle_rounds = 0;
        }
    }
}

}  // namespace ironpulse::ingest
