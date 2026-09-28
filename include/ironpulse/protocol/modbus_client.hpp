#pragma once

#include <asio.hpp>
#include <atomic>
#include <chrono>
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
/// The client owns a persistent connection. Modbus TCP is a strict
/// request/response protocol, so callers issue one request at a time and
/// wait for its callback before sending the next (DevicePoller does).
///
/// Every connect and request is bounded by a timeout: a device that
/// accepts the TCP connection but never answers would otherwise stall
/// polling forever. On timeout the connection is closed and the pending
/// callback receives a ModbusError.
///
/// Thread-safety: all I/O runs on an internal strand, so public methods
/// may be called from any thread running the io_context.
class ModbusClient : public std::enable_shared_from_this<ModbusClient> {
public:
    static std::shared_ptr<ModbusClient> create(asio::io_context& io_context,
                                                std::string host,
                                                std::uint16_t port,
                                                std::chrono::milliseconds timeout = std::chrono::seconds(3));

    void connect(std::function<void(bool success)> on_done);

    /// Reads `quantity` holding registers (function 0x03) starting at `start_address`.
    void read_holding_registers(std::uint8_t unit_id,
                                std::uint16_t start_address,
                                std::uint16_t quantity,
                                RegistersCallback callback);

    /// Reads `quantity` input registers (function 0x04) starting at `start_address`.
    void read_input_registers(std::uint8_t unit_id,
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
    ModbusClient(asio::io_context& io_context,
                 std::string host,
                 std::uint16_t port,
                 std::chrono::milliseconds timeout);

    void read_registers(FunctionCode function,
                        std::uint8_t unit_id,
                        std::uint16_t start_address,
                        std::uint16_t quantity,
                        RegistersCallback callback);
    void send_request(const ModbusRequest& request, RegistersCallback callback);
    void read_response(std::uint16_t transaction_id, RegistersCallback callback);
    void arm_deadline();
    void disarm_deadline();
    void close_socket();

    asio::strand<asio::io_context::executor_type> strand_;
    asio::ip::tcp::socket socket_;
    asio::ip::tcp::resolver resolver_;
    asio::steady_timer deadline_;
    std::string host_;
    std::uint16_t port_;
    std::chrono::milliseconds timeout_;
    std::atomic<bool> connected_{false};
    bool timed_out_ = false;
    std::uint64_t deadline_generation_ = 0;
    std::uint16_t next_transaction_id_ = 1;
    std::vector<std::uint8_t> read_buffer_;
};

}  // namespace ironpulse::protocol
