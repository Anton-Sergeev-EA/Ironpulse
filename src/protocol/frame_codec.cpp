#include "ironpulse/protocol/frame_codec.hpp"

namespace ironpulse::protocol {

namespace {
constexpr std::uint16_t kProtocolId = 0x0000;  // Modbus TCP is always 0
constexpr std::size_t kMbapHeaderLength = 7;   // transaction(2) + protocol(2) + length(2) + unit(1)

void push_u16(std::vector<std::uint8_t>& buf, std::uint16_t value) {
    buf.push_back(static_cast<std::uint8_t>(value >> 8));
    buf.push_back(static_cast<std::uint8_t>(value & 0xFF));
}

std::uint16_t read_u16(const std::vector<std::uint8_t>& buf, std::size_t offset) {
    return static_cast<std::uint16_t>((buf[offset] << 8) | buf[offset + 1]);
}
}  // namespace

std::vector<std::uint8_t> encode_request(const ModbusRequest& request) {
    // PDU: function code (1) + start address (2) + quantity/value (2) = 5 bytes
    constexpr std::uint16_t pdu_length = 5;
    constexpr std::uint16_t length_field = pdu_length + 1;  // + unit id byte

    std::vector<std::uint8_t> buf;
    buf.reserve(kMbapHeaderLength + pdu_length);

    push_u16(buf, request.transaction_id);
    push_u16(buf, kProtocolId);
    push_u16(buf, length_field);
    buf.push_back(request.unit_id);

    buf.push_back(static_cast<std::uint8_t>(request.function));
    push_u16(buf, request.start_address);
    push_u16(buf, request.quantity_or_value);

    return buf;
}

std::optional<std::size_t> expected_frame_length(const std::vector<std::uint8_t>& header_bytes) {
    if (header_bytes.size() < kMbapHeaderLength) {
        return std::nullopt;
    }
    const std::uint16_t length_field = read_u16(header_bytes, 4);
    // total = 6 bytes (transaction+protocol+length) + length_field (unit id + PDU)
    return static_cast<std::size_t>(6) + length_field;
}

std::optional<ModbusResponse> decode_response(const std::vector<std::uint8_t>& buffer) {
    auto total_length = expected_frame_length(buffer);
    if (!total_length || buffer.size() < *total_length) {
        return std::nullopt;
    }

    ModbusResponse response;
    response.transaction_id = read_u16(buffer, 0);
    response.unit_id = buffer[6];

    const std::uint8_t function_byte = buffer[7];
    constexpr std::uint8_t exception_bit = 0x80;
    response.is_exception = (function_byte & exception_bit) != 0;
    response.function = static_cast<FunctionCode>(function_byte & ~exception_bit);

    if (response.is_exception) {
        response.exception_code = buffer[8];
        return response;
    }

    switch (response.function) {
        case FunctionCode::read_holding_registers:
        case FunctionCode::read_input_registers: {
            const std::uint8_t byte_count = buffer[8];
            const std::size_t register_count = byte_count / 2;
            response.registers.reserve(register_count);
            for (std::size_t i = 0; i < register_count; ++i) {
                response.registers.push_back(read_u16(buffer, 9 + i * 2));
            }
            break;
        }
        case FunctionCode::write_single_register: {
            // Echo response: address (2) + value (2)
            response.registers.push_back(read_u16(buffer, 10));
            break;
        }
    }

    return response;
}

}  // namespace ironpulse::protocol
