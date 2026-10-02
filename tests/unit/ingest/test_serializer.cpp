#include <array>
#include <catch2/catch_test_macros.hpp>
#include <cstdint>
#include <cstring>
#include <span>
#include <string>
#include <vector>

#include "ironpulse/ingest/serializer.hpp"

using namespace ironpulse::ingest;

namespace {

std::vector<TelemetrySample> make_samples(std::uint32_t count) {
    std::vector<TelemetrySample> samples;
    for (std::uint32_t i = 0; i < count; ++i) {
        samples.push_back(TelemetrySample{
            .timestamp_ms = 1'700'000'000'000ULL + i,
            .value = static_cast<double>(i) * 2.5 - 1.0,
            .tag_id = i + 1,
            .quality = static_cast<std::uint8_t>(i % 2),
        });
    }
    return samples;
}

bool same(const TelemetrySample& a, const TelemetrySample& b) {
    return a.timestamp_ms == b.timestamp_ms && a.value == b.value && a.tag_id == b.tag_id &&
           a.quality == b.quality;
}

}  // namespace

TEST_CASE("CRC-32C matches the standard check value on every code path", "[ingest][crc]") {
    // CRC-32C("123456789") = 0xE3069283 (RFC 3720, appendix B.4).
    const std::string check = "123456789";
    const auto* bytes = reinterpret_cast<const std::uint8_t*>(check.data());
    CHECK(Serializer::crc32c_software(bytes, check.size()) == 0xE3069283U);
    CHECK(Serializer::crc32c(check.data(), check.size()) == 0xE3069283U);
    CHECK(Serializer::crc32c(nullptr, 0) == 0U);
}

TEST_CASE("Hardware and software CRC-32C agree for every length and alignment", "[ingest][crc]") {
    // The regression this guards against: the SSE4.2 path and the fallback
    // used different polynomials, so batches only verified on the machine
    // type that wrote them.
    std::vector<std::uint8_t> data(301);
    for (std::size_t i = 0; i < data.size(); ++i) {
        data[i] = static_cast<std::uint8_t>(i * 7 + 3);
    }
    for (std::size_t offset = 0; offset < 8; ++offset) {
        for (std::size_t length = 0; length + offset <= data.size(); length += 13) {
            const auto* start = data.data() + offset;
            INFO("offset " << offset << ", length " << length);
            CHECK(Serializer::crc32c(start, length) == Serializer::crc32c_software(start, length));
        }
    }
}

TEST_CASE("Serializer round-trips a batch through the in-place view", "[ingest][serializer]") {
    const auto samples = make_samples(5);
    alignas(8) std::array<std::uint8_t, 512> buffer{};

    const std::size_t written = Serializer::serialize(samples, buffer);
    REQUIRE(written == Serializer::required_buffer_size(samples.size()));
    CHECK(written == sizeof(BatchHeader) + 5 * 24);

    std::span<const TelemetrySample> view;
    REQUIRE(Serializer::decode_view(std::span<const std::uint8_t>(buffer.data(), written), view) ==
            DecodeStatus::ok);
    REQUIRE(view.size() == samples.size());
    for (std::size_t i = 0; i < samples.size(); ++i) {
        CHECK(same(view[i], samples[i]));
    }
}

TEST_CASE("Serializer places samples on an 8-byte boundary", "[ingest][serializer]") {
    // The original 12-byte header put the doubles 4 bytes off alignment.
    STATIC_CHECK(sizeof(BatchHeader) % alignof(TelemetrySample) == 0);
}

TEST_CASE("Serializer detects a damaged payload", "[ingest][serializer]") {
    auto buffer = Serializer::serialize(make_samples(3));
    std::span<const TelemetrySample> view;
    REQUIRE(Serializer::decode_view(buffer, view) == DecodeStatus::ok);

    buffer[sizeof(BatchHeader) + 2] ^= 0xFF;
    CHECK(Serializer::decode_view(buffer, view) == DecodeStatus::crc_mismatch);
    std::vector<TelemetrySample> copied;
    CHECK(Serializer::decode_copy(buffer, copied) == DecodeStatus::crc_mismatch);
    CHECK(copied.empty());
}

TEST_CASE("Serializer reports why a buffer is not a valid batch", "[ingest][serializer]") {
    const auto good = Serializer::serialize(make_samples(2));
    std::span<const TelemetrySample> view;

    SECTION("shorter than a header") {
        CHECK(Serializer::decode_view(std::span<const std::uint8_t>(good.data(), 10), view) ==
              DecodeStatus::too_short);
    }
    SECTION("samples cut off") {
        CHECK(Serializer::decode_view(std::span<const std::uint8_t>(good.data(), good.size() - 1), view) ==
              DecodeStatus::truncated);
    }
    SECTION("wrong magic") {
        auto bad = good;
        bad[0] ^= 0xFF;
        CHECK(Serializer::decode_view(bad, view) == DecodeStatus::bad_magic);
    }
    SECTION("newer format version") {
        auto bad = good;
        bad[4] = 99;
        CHECK(Serializer::decode_view(bad, view) == DecodeStatus::unsupported_version);
    }
}

TEST_CASE("Serializer refuses an in-place view of misaligned data but can copy it", "[ingest][serializer]") {
    const auto samples = make_samples(4);
    const auto encoded = Serializer::serialize(samples);
    std::vector<std::uint8_t> shifted(encoded.size() + 1);
    std::memcpy(shifted.data() + 1, encoded.data(), encoded.size());
    const std::span<const std::uint8_t> misaligned(shifted.data() + 1, encoded.size());

    std::span<const TelemetrySample> view;
    CHECK(Serializer::decode_view(misaligned, view) == DecodeStatus::misaligned);

    std::vector<TelemetrySample> copied;
    REQUIRE(Serializer::decode_copy(misaligned, copied) == DecodeStatus::ok);
    REQUIRE(copied.size() == samples.size());
    for (std::size_t i = 0; i < samples.size(); ++i) {
        CHECK(same(copied[i], samples[i]));
    }
}

TEST_CASE("Serializer writes nothing into a buffer that is too small", "[ingest][serializer]") {
    const auto samples = make_samples(1);
    std::array<std::uint8_t, sizeof(BatchHeader)> small{};
    CHECK(Serializer::serialize(samples, small) == 0);
    CHECK(small == std::array<std::uint8_t, sizeof(BatchHeader)>{});
}

TEST_CASE("Serializer encodes an empty batch", "[ingest][serializer]") {
    const auto encoded = Serializer::serialize(std::span<const TelemetrySample>{});
    REQUIRE(encoded.size() == sizeof(BatchHeader));
    std::vector<TelemetrySample> out;
    CHECK(Serializer::decode_copy(encoded, out) == DecodeStatus::ok);
    CHECK(out.empty());
}
