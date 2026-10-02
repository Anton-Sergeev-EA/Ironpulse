#pragma once

#include <atomic>
#include <cstddef>
#include <memory>
#include <new>
#include <stdexcept>
#include <type_traits>
#include <utility>

#include "ironpulse/ingest/cache_line.hpp"

namespace ironpulse::ingest {

/// Lock-free single-producer / single-consumer ring buffer.
///
/// Exactly one thread may call `push`/`emplace` and exactly one (other)
/// thread may call `pop` at any time. Callers with several producers must
/// serialise them themselves — see ReadingExporter, which does so with a
/// short critical section in front of the buffer.
///
/// Design notes:
/// - head and tail live on separate cache lines, so the producer and the
///   consumer never invalidate each other's line on every operation;
/// - each side keeps a cached copy of the other side's index and only
///   reloads it (an acquire load of a line owned by the other core) when
///   the cached value says the buffer looks full or empty;
/// - indices grow monotonically and are masked on access, so `tail - head`
///   is the size without a separate "wrapped" flag;
/// - elements are constructed in place in raw storage, so T need not be
///   default-constructible and move-only types work.
///
/// The capacity is chosen at run time (it comes from configuration) and
/// must be a power of two so that masking replaces a modulo.
template <typename T>
class SpscRingBuffer {
public:
    using value_type = T;
    using size_type = std::size_t;

    explicit SpscRingBuffer(size_type capacity) : capacity_(capacity), mask_(capacity - 1) {
        if (capacity < 2 || (capacity & (capacity - 1)) != 0) {
            throw std::invalid_argument("SpscRingBuffer capacity must be a power of two and at least 2");
        }
        buffer_ = static_cast<Slot*>(::operator new[](sizeof(Slot) * capacity_, std::align_val_t{alignof(Slot)}));
    }

    ~SpscRingBuffer() {
        // Destroy unconsumed elements in place. Not written as
        // "T dummy; while (pop(dummy)) {}", which would require T to be
        // default-constructible. No producer or consumer can be running by
        // the time the destructor is called, so relaxed loads suffice.
        if constexpr (!std::is_trivially_destructible_v<T>) {
            size_type head = head_.load(std::memory_order_relaxed);
            const size_type tail = tail_.load(std::memory_order_relaxed);
            for (; head != tail; ++head) {
                std::destroy_at(slot(head));
            }
        }
        ::operator delete[](buffer_, std::align_val_t{alignof(Slot)});
    }

    SpscRingBuffer(const SpscRingBuffer&) = delete;
    SpscRingBuffer& operator=(const SpscRingBuffer&) = delete;
    SpscRingBuffer(SpscRingBuffer&&) = delete;
    SpscRingBuffer& operator=(SpscRingBuffer&&) = delete;

    /// Constructs an element in place at the tail (producer thread only).
    /// Returns false, leaving the buffer unchanged, when it is full.
    template <typename... Args>
    bool emplace(Args&&... args) noexcept(std::is_nothrow_constructible_v<T, Args...>) {
        const size_type tail = tail_.load(std::memory_order_relaxed);
        if (tail - head_cached_ >= capacity_) {
            head_cached_ = head_.load(std::memory_order_acquire);
            if (tail - head_cached_ >= capacity_) {
                return false;
            }
        }
        ::new (static_cast<void*>(buffer_ + (tail & mask_))) T(std::forward<Args>(args)...);
        // Release: the consumer must see the constructed element before the new tail.
        tail_.store(tail + 1, std::memory_order_release);
        return true;
    }

    bool push(T&& item) noexcept(std::is_nothrow_move_constructible_v<T>) {
        return emplace(std::move(item));
    }

    bool push(const T& item) noexcept(std::is_nothrow_copy_constructible_v<T>) {
        return emplace(item);
    }

    /// Moves the oldest element into `out` (consumer thread only).
    /// Returns false when the buffer is empty.
    bool pop(T& out) noexcept(std::is_nothrow_move_assignable_v<T>) {
        const size_type head = head_.load(std::memory_order_relaxed);
        if (head == tail_cached_) {
            tail_cached_ = tail_.load(std::memory_order_acquire);
            if (head == tail_cached_) {
                return false;
            }
        }
        T* element = slot(head);
        out = std::move(*element);
        std::destroy_at(element);
        // Release: the producer must not reuse the slot before it is vacated.
        head_.store(head + 1, std::memory_order_release);
        return true;
    }

    /// Approximate number of queued elements; exact only when neither side
    /// is running concurrently.
    [[nodiscard]] size_type size() const noexcept {
        const size_type head = head_.load(std::memory_order_acquire);
        const size_type tail = tail_.load(std::memory_order_acquire);
        return tail - head;
    }

    [[nodiscard]] bool empty() const noexcept {
        return size() == 0;
    }

    [[nodiscard]] size_type capacity() const noexcept {
        return capacity_;
    }

private:
    struct Slot {
        alignas(T) std::byte bytes[sizeof(T)];
    };

    [[nodiscard]] T* slot(size_type index) noexcept {
        return std::launder(reinterpret_cast<T*>(buffer_[index & mask_].bytes));
    }

    const size_type capacity_;
    const size_type mask_;
    Slot* buffer_ = nullptr;

    // Producer-owned line: the tail it publishes and its cached view of head.
    alignas(kCacheLineSize) std::atomic<size_type> tail_{0};
    size_type head_cached_ = 0;

    // Consumer-owned line: the head it publishes and its cached view of tail.
    alignas(kCacheLineSize) std::atomic<size_type> head_{0};
    size_type tail_cached_ = 0;

    // Keeps whatever follows this object off the consumer's line.
    alignas(kCacheLineSize) std::byte padding_[1]{};
};

}  // namespace ironpulse::ingest
