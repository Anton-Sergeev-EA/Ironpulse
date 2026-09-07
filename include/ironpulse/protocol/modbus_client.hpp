#pragma once

#include <asio.hpp>
#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <variant>
#include <vector>

#include "ironpulse/protocol/frame_codec.hpp"

namespace ironpulse::protocol {

/// Result of a Modbus operation, following the Result<T, Error> idiom to
/// avoid exceptions on the hot path (network errors are expected, not
/// exceptional).
struct ModbusError {
    std::string message;
};

using RegistersResult = std::variant<std::vector<std::uint16_t>, ModbusError>;
using RegistersCallback = std::function<void(RegistersResult)>;

/// Asynchronous Modbus TCP client for a single remote device.
///
/// The client owns a persistent connection and serializes requests
/// internally (Modbus TCP is a strict request/response protocol — no
/// pipelining), so callers can safely issue calls back-to-back.
///
/// Thread-safety: all public methods must be called from a strand or from
/// the same thread that runs the owning io_context, per standard Asio
/// conventions.
class ModbusClient : public std::enable_shared_from_this<ModbusClient> {
public:
    static std::shared_ptr<ModbusClient> create(asio::io_context& io_context,
                                                std::string host,
                                                std::uint16_t port);

    void connect(std::function<void(bool success)> on_done);

    /// Reads `quantity` holding registers starting at `start_address`.
    void read_holding_registers(std::uint8_t unit_id,
                                std::uint16_t start_address,
                                std::uint16_t quantity,
                                RegistersCallback callback);

    void write_single_register(std::uint8_t unit_id,
                               std::uint16_t address,
                               std::uint16_t value,
                               std::function<void(bool success)> on_done);

    [[nodiscard]] bool is_connected() const noexcept {
        return connected_;
    }
    [[nodiscard]] const std::string& host() const noexcept {
        return host_;
    }

    void close();

private:
    ModbusClient(asio::io_context& io_context, std::string host, std::uint16_t port);

    void send_request(const ModbusRequest& request, RegistersCallback callback);
    void read_response(RegistersCallback callback);

    asio::io_context& io_context_;
    asio::ip::tcp::socket socket_;
    asio::ip::tcp::resolver resolver_;
    std::string host_;
    std::uint16_t port_;
    std::atomic<bool> connected_{false};
    std::uint16_t next_transaction_id_ = 1;
    std::vector<std::uint8_t> read_buffer_;
};

}  // namespace ironpulse::protocol
