#include "ironpulse/protocol/modbus_client.hpp"

#include "ironpulse/core/logger.hpp"

namespace ironpulse::protocol {

std::shared_ptr<ModbusClient> ModbusClient::create(asio::io_context& io_context,
                                                   std::string host,
                                                   std::uint16_t port,
                                                   std::chrono::milliseconds timeout) {
    // Cannot use std::make_shared with a private constructor directly;
    // this helper struct exposes it just for make_shared's internal new.
    struct EnableMakeShared : ModbusClient {
        EnableMakeShared(asio::io_context& ctx, std::string h, std::uint16_t p, std::chrono::milliseconds t)
            : ModbusClient(ctx, std::move(h), p, t) {}
    };
    return std::make_shared<EnableMakeShared>(io_context, std::move(host), port, timeout);
}

// The socket, resolver and timer all use the strand as their executor, so
// every completion handler (I/O and timeout alike) is serialized on it.
ModbusClient::ModbusClient(asio::io_context& io_context,
                           std::string host,
                           std::uint16_t port,
                           std::chrono::milliseconds timeout)
    : strand_(asio::make_strand(io_context)),
      socket_(strand_),
      resolver_(strand_),
      deadline_(strand_),
      host_(std::move(host)),
      port_(port),
      timeout_(timeout) {}

void ModbusClient::arm_deadline() {
    timed_out_ = false;
    const std::uint64_t generation = ++deadline_generation_;
    deadline_.expires_after(timeout_);
    auto self = shared_from_this();
    deadline_.async_wait([this, self, generation](const asio::error_code& ec) {
        // A cancelled wait, or one that expired at the same moment the
        // operation completed (its handler was already queued when
        // disarm_deadline() ran), must not touch the connection.
        if (ec || generation != deadline_generation_) {
            return;
        }
        timed_out_ = true;
        resolver_.cancel();
        close_socket();
    });
}

void ModbusClient::disarm_deadline() {
    ++deadline_generation_;
    deadline_.cancel();
}

void ModbusClient::close_socket() {
    asio::error_code ec;
    socket_.shutdown(asio::ip::tcp::socket::shutdown_both, ec);
    socket_.close(ec);
    connected_ = false;
}

void ModbusClient::connect(std::function<void(bool success)> on_done) {
    auto self = shared_from_this();
    asio::dispatch(strand_, [this, self, on_done = std::move(on_done)]() mutable {
        close_socket();
        arm_deadline();
        resolver_.async_resolve(
            host_,
            std::to_string(port_),
            [this, self, on_done = std::move(on_done)](
                const asio::error_code& ec, const asio::ip::tcp::resolver::results_type& endpoints) mutable {
                if (ec) {
                    disarm_deadline();
                    IP_LOG_DEBUG(
                        "Modbus resolve failed for {}: {}", host_, timed_out_ ? "timed out" : ec.message());
                    on_done(false);
                    return;
                }
                asio::async_connect(socket_,
                                    endpoints,
                                    [this, self, on_done = std::move(on_done)](
                                        const asio::error_code& connect_ec, const asio::ip::tcp::endpoint&) {
                                        disarm_deadline();
                                        if (connect_ec) {
                                            IP_LOG_DEBUG("Modbus connect failed for {}:{} — {}",
                                                         host_,
                                                         port_,
                                                         timed_out_ ? "timed out" : connect_ec.message());
                                            close_socket();
                                            on_done(false);
                                            return;
                                        }
                                        asio::error_code opt_ec;
                                        socket_.set_option(asio::ip::tcp::no_delay(true), opt_ec);
                                        connected_ = true;
                                        IP_LOG_DEBUG("Modbus client connected to {}:{}", host_, port_);
                                        on_done(true);
                                    });
            });
    });
}

void ModbusClient::read_holding_registers(std::uint8_t unit_id,
                                          std::uint16_t start_address,
                                          std::uint16_t quantity,
                                          RegistersCallback callback) {
    read_registers(
        FunctionCode::read_holding_registers, unit_id, start_address, quantity, std::move(callback));
}

void ModbusClient::read_input_registers(std::uint8_t unit_id,
                                        std::uint16_t start_address,
                                        std::uint16_t quantity,
                                        RegistersCallback callback) {
    read_registers(FunctionCode::read_input_registers, unit_id, start_address, quantity, std::move(callback));
}

void ModbusClient::read_registers(FunctionCode function,
                                  std::uint8_t unit_id,
                                  std::uint16_t start_address,
                                  std::uint16_t quantity,
                                  RegistersCallback callback) {
    auto self = shared_from_this();
    asio::dispatch(
        strand_,
        [this, self, function, unit_id, start_address, quantity, callback = std::move(callback)]() mutable {
            ModbusRequest request{
                .transaction_id = next_transaction_id_++,
                .unit_id = unit_id,
                .function = function,
                .start_address = start_address,
                .quantity_or_value = quantity,
            };
            send_request(request, [quantity, callback = std::move(callback)](RegistersResult result) {
                // A device answering with fewer registers than
                // asked for would otherwise surface as garbage
                // values further down the pipeline.
                if (auto* regs = std::get_if<std::vector<std::uint16_t>>(&result);
                    regs && regs->size() < quantity) {
                    callback(ModbusError{"short response: expected " + std::to_string(quantity) +
                                         " registers, got " + std::to_string(regs->size())});
                    return;
                }
                callback(std::move(result));
            });
        });
}

void ModbusClient::write_single_register(std::uint8_t unit_id,
                                         std::uint16_t address,
                                         std::uint16_t value,
                                         std::function<void(bool)> on_done) {
    auto self = shared_from_this();
    asio::dispatch(strand_, [this, self, unit_id, address, value, on_done = std::move(on_done)]() mutable {
        ModbusRequest request{
            .transaction_id = next_transaction_id_++,
            .unit_id = unit_id,
            .function = FunctionCode::write_single_register,
            .start_address = address,
            .quantity_or_value = value,
        };
        send_request(request, [on_done = std::move(on_done)](RegistersResult result) {
            on_done(std::holds_alternative<std::vector<std::uint16_t>>(result));
        });
    });
}

// Runs on the strand.
void ModbusClient::send_request(const ModbusRequest& request, RegistersCallback callback) {
    if (!connected_) {
        callback(ModbusError{"not connected"});
        return;
    }

    auto self = shared_from_this();
    auto buffer = std::make_shared<std::vector<std::uint8_t>>(encode_request(request));
    const std::uint16_t transaction_id = request.transaction_id;

    arm_deadline();
    asio::async_write(socket_,
                      asio::buffer(*buffer),
                      [this, self, buffer, transaction_id, callback = std::move(callback)](
                          const asio::error_code& ec, std::size_t) mutable {
                          if (ec) {
                              disarm_deadline();
                              const std::string reason = timed_out_ ? "timed out" : ec.message();
                              IP_LOG_DEBUG("Modbus write failed to {}: {}", host_, reason);
                              close_socket();
                              callback(ModbusError{"write failed: " + reason});
                              return;
                          }
                          read_response(transaction_id, std::move(callback));
                      });
}

// Runs on the strand.
void ModbusClient::read_response(std::uint16_t transaction_id, RegistersCallback callback) {
    read_buffer_.clear();
    read_buffer_.resize(7);  // MBAP header size

    auto self = shared_from_this();
    asio::async_read(
        socket_,
        asio::buffer(read_buffer_),
        [this, self, transaction_id, callback = std::move(callback)](const asio::error_code& ec,
                                                                     std::size_t) mutable {
            if (ec) {
                disarm_deadline();
                const std::string reason =
                    timed_out_ ? "no response within " + std::to_string(timeout_.count()) + " ms"
                               : ec.message();
                IP_LOG_DEBUG("Modbus header read failed from {}: {}", host_, reason);
                close_socket();
                callback(ModbusError{"read header failed: " + reason});
                return;
            }

            auto total_length = expected_frame_length(read_buffer_);
            // The MBAP length field covers unit id + PDU; a Modbus PDU is
            // at most 253 bytes, so anything larger is a corrupt stream.
            if (!total_length || *total_length < read_buffer_.size() + 1 || *total_length > 7 + 253) {
                disarm_deadline();
                close_socket();
                callback(ModbusError{"malformed MBAP header"});
                return;
            }

            const std::size_t remaining = *total_length - read_buffer_.size();
            const std::size_t previous_size = read_buffer_.size();
            read_buffer_.resize(*total_length);

            asio::async_read(socket_,
                             asio::buffer(read_buffer_.data() + previous_size, remaining),
                             [this, self, transaction_id, callback = std::move(callback)](
                                 const asio::error_code& body_ec, std::size_t) mutable {
                                 disarm_deadline();
                                 if (body_ec) {
                                     const std::string reason = timed_out_ ? "timed out" : body_ec.message();
                                     IP_LOG_DEBUG("Modbus body read failed from {}: {}", host_, reason);
                                     close_socket();
                                     callback(ModbusError{"read body failed: " + reason});
                                     return;
                                 }

                                 auto response = decode_response(read_buffer_);
                                 if (!response) {
                                     close_socket();
                                     callback(ModbusError{"failed to decode response"});
                                     return;
                                 }
                                 if (response->transaction_id != transaction_id) {
                                     // The stream is out of sync (e.g. a late answer to
                                     // a timed-out request); reconnecting is the only
                                     // safe way to realign it.
                                     close_socket();
                                     callback(ModbusError{"transaction id mismatch"});
                                     return;
                                 }
                                 if (response->is_exception) {
                                     callback(ModbusError{"device exception code " +
                                                          std::to_string(response->exception_code)});
                                     return;
                                 }
                                 callback(std::move(response->registers));
                             });
        });
}

void ModbusClient::close() {
    auto self = shared_from_this();
    asio::dispatch(strand_, [this, self] {
        disarm_deadline();
        close_socket();
    });
}

}  // namespace ironpulse::protocol
