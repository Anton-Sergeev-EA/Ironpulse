#include "ironpulse/protocol/modbus_client.hpp"

#include "ironpulse/core/logger.hpp"

namespace ironpulse::protocol {

std::shared_ptr<ModbusClient> ModbusClient::create(asio::io_context& io_context,
                                                   std::string host,
                                                   std::uint16_t port) {
    // Cannot use std::make_shared with a private constructor directly;
    // this helper struct exposes it just for make_shared's internal new.
    struct EnableMakeShared : ModbusClient {
        EnableMakeShared(asio::io_context& ctx, std::string h, std::uint16_t p)
            : ModbusClient(ctx, std::move(h), p) {}
    };
    return std::make_shared<EnableMakeShared>(io_context, std::move(host), port);
}

ModbusClient::ModbusClient(asio::io_context& io_context, std::string host, std::uint16_t port)
    : io_context_(io_context),
      socket_(io_context),
      resolver_(io_context),
      host_(std::move(host)),
      port_(port) {}

void ModbusClient::connect(std::function<void(bool success)> on_done) {
    auto self = shared_from_this();
    resolver_.async_resolve(
        host_,
        std::to_string(port_),
        [this, self, on_done = std::move(on_done)](const asio::error_code& ec,
                                                   const asio::ip::tcp::resolver::results_type& endpoints) {
            if (ec) {
                IP_LOG_ERROR("Modbus resolve failed for {}: {}", host_, ec.message());
                on_done(false);
                return;
            }
            asio::async_connect(
                socket_,
                endpoints,
                [this, self, on_done](const asio::error_code& connect_ec, const asio::ip::tcp::endpoint&) {
                    if (connect_ec) {
                        IP_LOG_ERROR(
                            "Modbus connect failed for {}:{} — {}", host_, port_, connect_ec.message());
                        connected_ = false;
                        on_done(false);
                        return;
                    }
                    asio::error_code opt_ec;
                    socket_.set_option(asio::ip::tcp::no_delay(true), opt_ec);
                    connected_ = true;
                    IP_LOG_INFO("Modbus client connected to {}:{}", host_, port_);
                    on_done(true);
                });
        });
}

void ModbusClient::read_holding_registers(std::uint8_t unit_id,
                                          std::uint16_t start_address,
                                          std::uint16_t quantity,
                                          RegistersCallback callback) {
    ModbusRequest request{
        .transaction_id = next_transaction_id_++,
        .unit_id = unit_id,
        .function = FunctionCode::read_holding_registers,
        .start_address = start_address,
        .quantity_or_value = quantity,
    };
    send_request(request, std::move(callback));
}

void ModbusClient::write_single_register(std::uint8_t unit_id,
                                         std::uint16_t address,
                                         std::uint16_t value,
                                         std::function<void(bool)> on_done) {
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
}

void ModbusClient::send_request(const ModbusRequest& request, RegistersCallback callback) {
    if (!connected_) {
        callback(ModbusError{"not connected"});
        return;
    }

    auto self = shared_from_this();
    auto buffer = std::make_shared<std::vector<std::uint8_t>>(encode_request(request));

    asio::async_write(socket_,
                      asio::buffer(*buffer),
                      [this, self, buffer, callback = std::move(callback)](const asio::error_code& ec,
                                                                           std::size_t) mutable {
                          if (ec) {
                              IP_LOG_ERROR("Modbus write failed to {}: {}", host_, ec.message());
                              connected_ = false;
                              callback(ModbusError{"write failed: " + ec.message()});
                              return;
                          }
                          read_response(std::move(callback));
                      });
}

void ModbusClient::read_response(RegistersCallback callback) {
    read_buffer_.clear();
    read_buffer_.resize(7);  // MBAP header size

    auto self = shared_from_this();
    asio::async_read(
        socket_,
        asio::buffer(read_buffer_),
        [this, self, callback = std::move(callback)](const asio::error_code& ec, std::size_t) mutable {
            if (ec) {
                IP_LOG_ERROR("Modbus header read failed from {}: {}", host_, ec.message());
                connected_ = false;
                callback(ModbusError{"read header failed: " + ec.message()});
                return;
            }

            auto total_length = expected_frame_length(read_buffer_);
            if (!total_length) {
                callback(ModbusError{"malformed MBAP header"});
                return;
            }

            const std::size_t remaining = *total_length - read_buffer_.size();
            const std::size_t previous_size = read_buffer_.size();
            read_buffer_.resize(*total_length);

            asio::async_read(
                socket_,
                asio::buffer(read_buffer_.data() + previous_size, remaining),
                [this, self, callback = std::move(callback)](const asio::error_code& body_ec,
                                                             std::size_t) mutable {
                    if (body_ec) {
                        IP_LOG_ERROR("Modbus body read failed from {}: {}", host_, body_ec.message());
                        connected_ = false;
                        callback(ModbusError{"read body failed: " + body_ec.message()});
                        return;
                    }

                    auto response = decode_response(read_buffer_);
                    if (!response) {
                        callback(ModbusError{"failed to decode response"});
                        return;
                    }
                    if (response->is_exception) {
                        callback(
                            ModbusError{"device exception code " + std::to_string(response->exception_code)});
                        return;
                    }
                    callback(response->registers);
                });
        });
}

void ModbusClient::close() {
    if (!connected_) {
        return;
    }
    asio::error_code ec;
    socket_.shutdown(asio::ip::tcp::socket::shutdown_both, ec);
    socket_.close(ec);
    connected_ = false;
}

}  // namespace ironpulse::protocol
