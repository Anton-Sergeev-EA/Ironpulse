#include <catch2/catch_test_macros.hpp>

#include "ironpulse/storage/ring_buffer.hpp"

using ironpulse::storage::RingBuffer;

TEST_CASE("RingBuffer reports the latest pushed value", "[ring_buffer]") {
    RingBuffer<int> buf(3);
    buf.push(1);
    buf.push(2);
    buf.push(3);

    auto latest = buf.latest();
    REQUIRE(latest.has_value());
    CHECK(latest->value == 3);
}

TEST_CASE("RingBuffer overwrites the oldest value once capacity is exceeded", "[ring_buffer]") {
    RingBuffer<int> buf(3);
    buf.push(1);
    buf.push(2);
    buf.push(3);
    buf.push(4);  // should overwrite 1

    CHECK(buf.size() == 3);
    auto samples = buf.recent(3);
    REQUIRE(samples.size() == 3);
    CHECK(samples[0].value == 2);
    CHECK(samples[1].value == 3);
    CHECK(samples[2].value == 4);
}

TEST_CASE("RingBuffer.recent caps the count at the current size", "[ring_buffer]") {
    RingBuffer<int> buf(10);
    buf.push(1);
    buf.push(2);

    auto samples = buf.recent(100);
    CHECK(samples.size() == 2);
}

TEST_CASE("RingBuffer starts empty", "[ring_buffer]") {
    RingBuffer<double> buf(5);
    CHECK(buf.size() == 0);
    CHECK_FALSE(buf.latest().has_value());
}
