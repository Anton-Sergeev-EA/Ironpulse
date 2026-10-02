#include <atomic>
#include <catch2/catch_test_macros.hpp>
#include <cstddef>
#include <memory>
#include <stdexcept>
#include <thread>
#include <vector>

#include "ironpulse/ingest/ring_buffer.hpp"

using ironpulse::ingest::SpscRingBuffer;

namespace {

/// Move-only and not default-constructible: the buffer must cope with both.
struct MoveOnly {
    explicit MoveOnly(int v) : value(v) {}
    MoveOnly(const MoveOnly&) = delete;
    MoveOnly& operator=(const MoveOnly&) = delete;
    MoveOnly(MoveOnly&&) noexcept = default;
    MoveOnly& operator=(MoveOnly&&) noexcept = default;
    int value;
};

/// Counts live instances, to prove the destructor cleans up queued elements.
struct Tracked {
    explicit Tracked(std::atomic<int>& live) : live_(&live) {
        live_->fetch_add(1);
    }
    Tracked(Tracked&& other) noexcept : live_(other.live_) {
        live_->fetch_add(1);
    }
    Tracked& operator=(Tracked&& other) noexcept {
        live_ = other.live_;
        return *this;
    }
    Tracked(const Tracked&) = delete;
    Tracked& operator=(const Tracked&) = delete;
    ~Tracked() {
        live_->fetch_sub(1);
    }
    std::atomic<int>* live_;
};

}  // namespace

TEST_CASE("SpscRingBuffer pops elements in FIFO order", "[ingest][spsc]") {
    SpscRingBuffer<int> rb(4);
    CHECK(rb.capacity() == 4);
    CHECK(rb.empty());

    CHECK(rb.push(1));
    CHECK(rb.push(2));
    CHECK(rb.push(3));
    CHECK(rb.size() == 3);

    int value = 0;
    REQUIRE(rb.pop(value));
    CHECK(value == 1);
    REQUIRE(rb.pop(value));
    CHECK(value == 2);
    REQUIRE(rb.pop(value));
    CHECK(value == 3);
    CHECK_FALSE(rb.pop(value));
    CHECK(rb.empty());
}

TEST_CASE("SpscRingBuffer rejects pushes when full and accepts them again after a pop", "[ingest][spsc]") {
    SpscRingBuffer<int> rb(4);
    for (int i = 0; i < 4; ++i) {
        REQUIRE(rb.push(i));
    }
    CHECK_FALSE(rb.push(99));
    CHECK(rb.size() == 4);

    int value = -1;
    REQUIRE(rb.pop(value));
    CHECK(value == 0);
    CHECK(rb.push(4));
}

TEST_CASE("SpscRingBuffer requires a power-of-two capacity of at least 2", "[ingest][spsc]") {
    CHECK_THROWS_AS(SpscRingBuffer<int>(0), std::invalid_argument);
    CHECK_THROWS_AS(SpscRingBuffer<int>(1), std::invalid_argument);
    CHECK_THROWS_AS(SpscRingBuffer<int>(6), std::invalid_argument);
    CHECK_NOTHROW(SpscRingBuffer<int>(2));
    CHECK_NOTHROW(SpscRingBuffer<int>(1024));
}

TEST_CASE("SpscRingBuffer supports move-only, non-default-constructible types", "[ingest][spsc]") {
    SpscRingBuffer<MoveOnly> rb(8);
    CHECK(rb.emplace(100));
    CHECK(rb.emplace(200));
    CHECK(rb.push(MoveOnly(300)));

    MoveOnly out(0);
    REQUIRE(rb.pop(out));
    CHECK(out.value == 100);
    REQUIRE(rb.pop(out));
    CHECK(out.value == 200);
    REQUIRE(rb.pop(out));
    CHECK(out.value == 300);
}

TEST_CASE("SpscRingBuffer keeps order across many wrap-arounds", "[ingest][spsc]") {
    SpscRingBuffer<int> rb(4);
    for (int cycle = 0; cycle < 1000; ++cycle) {
        REQUIRE(rb.push(cycle));
        REQUIRE(rb.push(cycle + 1));
        int a = -1;
        int b = -1;
        REQUIRE(rb.pop(a));
        REQUIRE(rb.pop(b));
        CHECK(a == cycle);
        CHECK(b == cycle + 1);
    }
    CHECK(rb.empty());
}

TEST_CASE("SpscRingBuffer destroys elements still queued when it is destroyed", "[ingest][spsc]") {
    std::atomic<int> live{0};
    {
        SpscRingBuffer<Tracked> rb(8);
        REQUIRE(rb.emplace(live));
        REQUIRE(rb.emplace(live));
        REQUIRE(rb.emplace(live));
        CHECK(live.load() == 3);
    }
    CHECK(live.load() == 0);
}

TEST_CASE("SpscRingBuffer delivers every element in order between two threads", "[ingest][spsc][concurrency]") {
    constexpr std::size_t kCount = 1'000'000;
    SpscRingBuffer<std::size_t> rb(1024);
    std::vector<std::size_t> received;
    received.reserve(kCount);

    std::thread producer([&] {
        for (std::size_t i = 0; i < kCount; ++i) {
            while (!rb.push(i)) {
                std::this_thread::yield();
            }
        }
    });
    std::thread consumer([&] {
        std::size_t value = 0;
        while (received.size() < kCount) {
            if (rb.pop(value)) {
                received.push_back(value);
            } else {
                std::this_thread::yield();
            }
        }
    });
    producer.join();
    consumer.join();

    REQUIRE(received.size() == kCount);
    bool in_order = true;
    for (std::size_t i = 0; i < kCount; ++i) {
        in_order = in_order && received[i] == i;
    }
    CHECK(in_order);
}
