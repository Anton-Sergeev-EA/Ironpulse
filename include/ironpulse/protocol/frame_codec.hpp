#pragma once

#include <cstdint>
#include <optional>
#include <vector>

namespace ironpulse::protocol {

/// Modbus function codes used by this client (subset).
enum class FunctionCode : std::uint8_t {
    read_holding_registers = 0x03,
    read_input_registers = 0x04,
    write_single_register = 0x06,
};

/// A single Modbus TCP request, encoded as MBAP header + PDU.
struct ModbusRequest {
    std::uint16_t transaction_id;
    std::uint8_t unit_id;
    FunctionCode function;
    std::uint16_t start_address;
    std::uint16_t quantity_or_value;
};

/// A decoded Modbus TCP response.
struct ModbusResponse {
    std::uint16_t transaction_id = 0;
    std::uint8_t unit_id = 0;
    FunctionCode function{};
    bool is_exception = false;
    std::uint8_t exception_code = 0;
    std::vector<std::uint16_t> registers;
};

/// Encodes a request into raw bytes ready to be sent over a TCP socket.
std::vector<std::uint8_t> encode_request(const ModbusRequest& request);

/// Decodes a raw byte buffer into a ModbusResponse.
/// Returns std::nullopt if the buffer does not contain a full, well-formed frame.
std::optional<ModbusResponse> decode_response(const std::vector<std::uint8_t>& buffer);

/// Returns the expected total frame length (MBAP header + PDU) once the
/// 7-byte MBAP header has been read, or std::nullopt if not enough bytes yet.
std::optional<std::size_t> expected_frame_length(const std::vector<std::uint8_t>& header_bytes);

}  // namespace ironpulse::protocol
