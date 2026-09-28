#include <bit>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <cmath>
#include <limits>

#include "ironpulse/protocol/register_decoder.hpp"

using Catch::Matchers::WithinAbs;
using ironpulse::core::DataType;
using ironpulse::core::RegisterType;
using ironpulse::core::SensorConfig;
using ironpulse::core::WordOrder;
using ironpulse::protocol::decode_raw;
using ironpulse::protocol::extract_value;
using ironpulse::protocol::plan_reads;
using ironpulse::protocol::ReadBlock;

namespace {

SensorConfig sensor(const std::string& id,
                    std::uint16_t address,
                    DataType type = DataType::uint16,
                    RegisterType reg = RegisterType::holding) {
    SensorConfig s;
    s.id = id;
    s.address = address;
    s.data_type = type;
    s.register_type = reg;
    return s;
}

}  // namespace

TEST_CASE("decode_raw interprets 16-bit registers", "[decoder]") {
    const std::uint16_t positive[] = {1234};
    const std::uint16_t negative[] = {0xFFF6};  // -10 as int16

    CHECK(decode_raw(positive, DataType::uint16, WordOrder::big) == 1234.0);
    CHECK(decode_raw(negative, DataType::uint16, WordOrder::big) == 65526.0);
    CHECK(decode_raw(negative, DataType::int16, WordOrder::big) == -10.0);
}

TEST_CASE("decode_raw honours word order for 32-bit values", "[decoder]") {
    const std::uint16_t abcd[] = {0x0001, 0x0002};  // big: 0x00010002

    CHECK(decode_raw(abcd, DataType::uint32, WordOrder::big) == 65538.0);
    CHECK(decode_raw(abcd, DataType::uint32, WordOrder::little) == 131073.0);

    const std::uint16_t minus_two[] = {0xFFFF, 0xFFFE};
    CHECK(decode_raw(minus_two, DataType::int32, WordOrder::big) == -2.0);
}

TEST_CASE("decode_raw decodes IEEE-754 float32 in both word orders", "[decoder]") {
    const auto bits = std::bit_cast<std::uint32_t>(23.75F);
    const std::uint16_t big[] = {static_cast<std::uint16_t>(bits >> 16),
                                 static_cast<std::uint16_t>(bits & 0xFFFF)};
    const std::uint16_t little[] = {big[1], big[0]};

    CHECK(decode_raw(big, DataType::float32, WordOrder::big) == 23.75);
    CHECK(decode_raw(little, DataType::float32, WordOrder::little) == 23.75);
}

TEST_CASE("extract_value applies scale and offset relative to the block start", "[decoder]") {
    SensorConfig s = sensor("t", 12, DataType::int16);
    s.scale = 0.1;
    s.offset = -40.0;
    ReadBlock block{RegisterType::holding, 10, 4, {0}};
    const std::vector<std::uint16_t> regs{0, 0, 650, 0};  // address 12 -> 650

    const auto value = extract_value(block, regs, s);
    REQUIRE(value);
    CHECK_THAT(*value, WithinAbs(25.0, 1e-9));
}

TEST_CASE("extract_value rejects short responses and non-finite floats", "[decoder]") {
    const SensorConfig s = sensor("f", 0, DataType::float32);
    ReadBlock block{RegisterType::holding, 0, 2, {0}};

    const std::vector<std::uint16_t> too_short{0x4000};
    CHECK_FALSE(extract_value(block, too_short, s));

    const auto nan_bits = std::bit_cast<std::uint32_t>(std::numeric_limits<float>::quiet_NaN());
    const std::vector<std::uint16_t> nan{static_cast<std::uint16_t>(nan_bits >> 16),
                                         static_cast<std::uint16_t>(nan_bits & 0xFFFF)};
    CHECK_FALSE(extract_value(block, nan, s));
}

TEST_CASE("plan_reads merges nearby registers into one request", "[decoder][planner]") {
    const std::vector<SensorConfig> sensors{
        sensor("a", 0, DataType::float32),  // 0-1
        sensor("b", 2),                     // 2
        sensor("c", 5),                     // 5 (gap of 2)
    };
    const auto blocks = plan_reads(sensors);

    REQUIRE(blocks.size() == 1);
    CHECK(blocks[0].start_address == 0);
    CHECK(blocks[0].quantity == 6);
    CHECK(blocks[0].sensor_indices.size() == 3);
}

TEST_CASE("plan_reads splits on large gaps, register types and the 125-register limit",
          "[decoder][planner]") {
    SECTION("large gap") {
        const auto blocks = plan_reads({sensor("a", 0), sensor("b", 100)}, 125, 8);
        CHECK(blocks.size() == 2);
    }
    SECTION("holding and input registers never share a request") {
        const auto blocks =
            plan_reads({sensor("a", 0), sensor("b", 1, DataType::uint16, RegisterType::input)});
        REQUIRE(blocks.size() == 2);
        CHECK(blocks[0].register_type == RegisterType::holding);
        CHECK(blocks[1].register_type == RegisterType::input);
    }
    SECTION("block size limit") {
        std::vector<SensorConfig> sensors;
        for (std::uint16_t i = 0; i < 130; ++i) {
            sensors.push_back(sensor("s" + std::to_string(i), i));
        }
        const auto blocks = plan_reads(sensors);
        REQUIRE(blocks.size() == 2);
        CHECK(blocks[0].quantity == 125);
        CHECK(blocks[1].start_address == 125);
        CHECK(blocks[1].quantity == 5);
    }
    SECTION("sensors listed out of order are still grouped by address") {
        const auto blocks = plan_reads({sensor("late", 3), sensor("early", 1)});
        REQUIRE(blocks.size() == 1);
        CHECK(blocks[0].start_address == 1);
        CHECK(blocks[0].quantity == 3);
    }
}
