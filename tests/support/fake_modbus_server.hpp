#pragma once

#include <asio.hpp>
#include <atomic>
#include <cstdint>
#include <future>
#include <map>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

namespace ironpulse::testing {

/// A small in-process Modbus TCP server for tests: answers function codes
/// 0x03 (holding) and 0x04 (input) from in-memory register maps, can be
/// told to go silent (accept but never answer) to exercise timeouts, and
/// runs on its own thread with its own io_context.
class FakeModbusServer {
public:
    FakeModbusServer() : acceptor_(io_, asio::ip::tcp::endpoint(asio::ip::address_v4::loopback(), 0)) {
        port_ = acceptor_.local_endpoint().port();
        accept();
        thread_ = std::thread([this] { io_.run(); });
    }

    ~FakeModbusServer() {
        stop();
    }

    FakeModbusServer(const FakeModbusServer&) = delete;
    FakeModbusServer& operator=(const FakeModbusServer&) = delete;

    void stop() {
        if (stopped_.exchange(true)) {
            return;
        }
        // Close everything on the server's own thread and wait for it, so
        // clients observe the disconnect before this returns.
        std::promise<void> closed;
        asio::post(io_, [this, &closed] {
            asio::error_code ec;
            acceptor_.close(ec);
            for (auto& socket : sockets_) {
                socket->close(ec);
            }
            closed.set_value();
        });
        closed.get_future().wait();
        io_.stop();
        if (thread_.joinable()) {
            thread_.join();
        }
    }

    [[nodiscard]] std::uint16_t port() const noexcept {
        return port_;
    }

    void set_holding(std::uint16_t address, std::uint16_t value) {
        std::lock_guard lock(mutex_);
        holding_[address] = value;
    }

    void set_input(std::uint16_t address, std::uint16_t value) {
        std::lock_guard lock(mutex_);
        input_[address] = value;
    }

    /// When true, requests are read but never answered.
    void set_silent(bool silent) {
        silent_ = silent;
    }

    [[nodiscard]] int requests_served() const noexcept {
        return requests_;
    }

private:
    void accept() {
        acceptor_.async_accept([this](const asio::error_code& ec, asio::ip::tcp::socket socket) {
            if (ec) {
                return;
            }
            auto shared = std::make_shared<asio::ip::tcp::socket>(std::move(socket));
            sockets_.push_back(shared);
            read_request(shared);
            accept();
        });
    }

    void read_request(const std::shared_ptr<asio::ip::tcp::socket>& socket) {
        auto buffer = std::make_shared<std::array<std::uint8_t, 12>>();
        asio::async_read(
            *socket, asio::buffer(*buffer), [this, socket, buffer](const asio::error_code& ec, std::size_t) {
                if (ec) {
                    return;
                }
                if (silent_) {
                    read_request(socket);
                    return;
                }
                const auto& b = *buffer;
                const std::uint8_t unit = b[6];
                const std::uint8_t function = b[7];
                const auto start = static_cast<std::uint16_t>((b[8] << 8) | b[9]);
                const auto quantity = static_cast<std::uint16_t>((b[10] << 8) | b[11]);

                std::vector<std::uint8_t> pdu;
                if (function == 0x03 || function == 0x04) {
                    pdu.push_back(function);
                    pdu.push_back(static_cast<std::uint8_t>(quantity * 2));
                    std::lock_guard lock(mutex_);
                    const auto& table = function == 0x03 ? holding_ : input_;
                    for (std::uint16_t i = 0; i < quantity; ++i) {
                        auto it = table.find(static_cast<std::uint16_t>(start + i));
                        const std::uint16_t value = it == table.end() ? 0 : it->second;
                        pdu.push_back(static_cast<std::uint8_t>(value >> 8));
                        pdu.push_back(static_cast<std::uint8_t>(value & 0xFF));
                    }
                } else {
                    pdu = {static_cast<std::uint8_t>(function | 0x80), 0x01};  // illegal function
                }

                auto response = std::make_shared<std::vector<std::uint8_t>>();
                response->push_back(b[0]);  // transaction id
                response->push_back(b[1]);
                response->push_back(0);  // protocol id
                response->push_back(0);
                const auto length = static_cast<std::uint16_t>(pdu.size() + 1);
                response->push_back(static_cast<std::uint8_t>(length >> 8));
                response->push_back(static_cast<std::uint8_t>(length & 0xFF));
                response->push_back(unit);
                response->insert(response->end(), pdu.begin(), pdu.end());
                ++requests_;

                asio::async_write(*socket,
                                  asio::buffer(*response),
                                  [this, socket, response](const asio::error_code& wec, std::size_t) {
                                      if (!wec) {
                                          read_request(socket);
                                      }
                                  });
            });
    }

    asio::io_context io_;
    asio::ip::tcp::acceptor acceptor_;
    std::uint16_t port_ = 0;
    std::thread thread_;
    std::vector<std::shared_ptr<asio::ip::tcp::socket>> sockets_;
    std::mutex mutex_;
    std::map<std::uint16_t, std::uint16_t> holding_;
    std::map<std::uint16_t, std::uint16_t> input_;
    std::atomic<bool> silent_{false};
    std::atomic<bool> stopped_{false};
    std::atomic<int> requests_{0};
};

}  // namespace ironpulse::testing
