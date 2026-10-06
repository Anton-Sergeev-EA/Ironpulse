#include <catch2/catch_test_macros.hpp>

#include "ironpulse/protocol/frame_codec.hpp"

using namespace ironpulse::protocol;

TEST_CASE("encode_request produces a well-formed MBAP + PDU frame", "[frame_codec]") {
    ModbusRequest request{
        .transaction_id = 0x0001,
        .unit_id = 0x11,
        .function = FunctionCode::read_holding_registers,
        .start_address = 0x006B,
        .quantity_or_value = 0x0003,
    };

    auto bytes = encode_request(request);

    REQUIRE(bytes.size() == 12);
    CHECK(bytes[0] == 0x00);   // transaction id hi
    CHECK(bytes[1] == 0x01);   // transaction id lo
    CHECK(bytes[2] == 0x00);   // protocol id hi
    CHECK(bytes[3] == 0x00);   // protocol id lo
    CHECK(bytes[4] == 0x00);   // length hi
    CHECK(bytes[5] == 0x06);   // length lo (unit + function + addr + qty)
    CHECK(bytes[6] == 0x11);   // unit id
    CHECK(bytes[7] == 0x03);   // function code
    CHECK(bytes[8] == 0x00);   // start addr hi
    CHECK(bytes[9] == 0x6B);   // start addr lo
    CHECK(bytes[10] == 0x00);  // quantity hi
    CHECK(bytes[11] == 0x03);  // quantity lo
}

TEST_CASE("expected_frame_length returns nullopt for incomplete header", "[frame_codec]") {
    std::vector<std::uint8_t> partial{0x00, 0x01, 0x00};
    CHECK_FALSE(expected_frame_length(partial).has_value());
}

TEST_CASE("decode_response parses a valid read_holding_registers reply", "[frame_codec]") {
    // Transaction 1, unit 0x11, 2 registers: 0x1234, 0x5678
    std::vector<std::uint8_t> raw{
        0x00,
        0x01,  // transaction id
        0x00,
        0x00,  // protocol id
        0x00,
        0x07,  // length = unit(1) + func(1) + bytecount(1) + 2*regs(4)
        0x11,  // unit id
        0x03,  // function code
        0x04,  // byte count
        0x12,
        0x34,  // register 1
        0x56,
        0x78,  // register 2
    };

    auto response = decode_response(raw);
    REQUIRE(response.has_value());
    CHECK(response->transaction_id == 1);
    CHECK(response->unit_id == 0x11);
    CHECK_FALSE(response->is_exception);
    REQUIRE(response->registers.size() == 2);
    CHECK(response->registers[0] == 0x1234);
    CHECK(response->registers[1] == 0x5678);
}

TEST_CASE("decode_response detects exception responses", "[frame_codec]") {
    std::vector<std::uint8_t> raw{
        0x00,
        0x02,  // transaction id
        0x00,
        0x00,  // protocol id
        0x00,
        0x03,  // length = unit(1) + func(1) + exception_code(1)
        0x11,  // unit id
        0x83,  // function code with exception bit set (0x03 | 0x80)
        0x02,  // exception code: illegal data address
    };

    auto response = decode_response(raw);
    REQUIRE(response.has_value());
    CHECK(response->is_exception);
    CHECK(response->exception_code == 0x02);
}

TEST_CASE("decode_response returns nullopt when buffer is shorter than declared length", "[frame_codec]") {
    std::vector<std::uint8_t> truncated{0x00, 0x01, 0x00, 0x00, 0x00, 0x07, 0x11, 0x03};
    CHECK_FALSE(decode_response(truncated).has_value());
}

TEST_CASE("decode_response rejects malformed payloads before indexing", "[frame_codec]") {
    const std::vector<std::vector<std::uint8_t>> malformed{
        {0, 1, 0, 0, 0, 1, 1},            // unit only, no function
        {0, 1, 0, 0, 0, 2, 1, 3},         // read function without byte count
        {0, 1, 0, 0, 0, 3, 1, 3, 250},    // count exceeds actual PDU (original heap overflow)
        {0, 1, 0, 0, 0, 4, 1, 3, 1, 42},  // odd register byte count
        {0, 1, 0, 0, 0, 3, 1, 3, 0},      // zero registers
        {0, 1, 0, 0, 0, 2, 1, 0x83},      // exception without exception code
        {0, 1, 0, 0, 0, 3, 1, 6, 0},      // truncated write echo
        {0, 1, 0, 0, 0, 3, 1, 0x7f, 0},   // unsupported function
    };
    for (const auto& frame : malformed) {
        CAPTURE(frame);
        CHECK_FALSE(decode_response(frame).has_value());
    }
}

TEST_CASE("MBAP rejects another protocol and lengths outside Modbus bounds", "[frame_codec]") {
    CHECK_FALSE(expected_frame_length({0, 1, 0, 1, 0, 7, 1}).has_value());
    CHECK_FALSE(expected_frame_length({0, 1, 0, 0, 0, 0, 1}).has_value());
    CHECK_FALSE(expected_frame_length({0, 1, 0, 0, 0, 1, 1}).has_value());
    CHECK_FALSE(expected_frame_length({0, 1, 0, 0, 0, 255, 1}).has_value());
    CHECK(expected_frame_length({0, 1, 0, 0, 0, 254, 1}) == 260);
}

TEST_CASE("every truncated valid reply and trailing garbage are rejected", "[frame_codec]") {
    const std::vector<std::uint8_t> valid{0, 1, 0, 0, 0, 7, 1, 3, 4, 0x12, 0x34, 0x56, 0x78};
    REQUIRE(decode_response(valid).has_value());
    for (std::size_t size = 0; size < valid.size(); ++size) {
        CAPTURE(size);
        auto truncated = valid;
        truncated.resize(size);
        CHECK_FALSE(decode_response(truncated).has_value());
    }
    auto extra = valid;
    extra.push_back(0);
    CHECK_FALSE(decode_response(extra).has_value());
}
