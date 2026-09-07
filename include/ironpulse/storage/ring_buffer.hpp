#pragma once

#include <chrono>
#include <mutex>
#include <optional>
#include <vector>

namespace ironpulse::storage {

/// A single timestamped sample.
template <typename T>
struct Sample {
    std::chrono::system_clock::time_point timestamp;
    T value;
};

/// Fixed-capacity, thread-safe circular buffer holding the most recent
/// samples for one sensor/series in memory. Older samples are overwritten
/// once capacity is reached — durable persistence is handled separately by
/// the WAL writer, which drains this buffer periodically.
///
/// This is intentionally simple (mutex-protected, not lock-free): for the
/// expected update rates (per-sensor, sub-kHz) a mutex is not the
/// bottleneck, and correctness/readability wins over a lock-free
/// implementation until profiling proves otherwise.
template <typename T>
class RingBuffer {
public:
    explicit RingBuffer(std::size_t capacity) : capacity_(capacity), data_(capacity) {}

    void push(T value, std::chrono::system_clock::time_point timestamp = std::chrono::system_clock::now()) {
        std::lock_guard lock(mutex_);
        data_[write_index_] = Sample<T>{timestamp, std::move(value)};
        write_index_ = (write_index_ + 1) % capacity_;
        if (size_ < capacity_) {
            ++size_;
        }
    }

    /// Returns the most recently pushed sample, if any.
    [[nodiscard]] std::optional<Sample<T>> latest() const {
        std::lock_guard lock(mutex_);
        if (size_ == 0) {
            return std::nullopt;
        }
        const std::size_t last_index = (write_index_ + capacity_ - 1) % capacity_;
        return data_[last_index];
    }

    /// Returns up to `count` most recent samples, oldest first.
    [[nodiscard]] std::vector<Sample<T>> recent(std::size_t count) const {
        std::lock_guard lock(mutex_);
        count = std::min(count, size_);
        std::vector<Sample<T>> result;
        result.reserve(count);
        for (std::size_t i = 0; i < count; ++i) {
            const std::size_t index = (write_index_ + capacity_ - count + i) % capacity_;
            result.push_back(data_[index]);
        }
        return result;
    }

    [[nodiscard]] std::size_t size() const {
        std::lock_guard lock(mutex_);
        return size_;
    }

    [[nodiscard]] std::size_t capacity() const noexcept {
        return capacity_;
    }

private:
    mutable std::mutex mutex_;
    std::size_t capacity_;
    std::vector<Sample<T>> data_;
    std::size_t write_index_ = 0;
    std::size_t size_ = 0;
};

}  // namespace ironpulse::storage
