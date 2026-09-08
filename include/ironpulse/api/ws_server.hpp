#pragma once

#include <array>
#include <asio.hpp>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace ironpulse::api {

/// A single accepted WebSocket connection. Handles the RFC 6455 opening
/// handshake, then supports server->client text frames (broadcast) and
/// reads client frames only to detect ping/close (this server never
/// expects meaningful data *from* the dashboard, so incoming text frames
/// are simply discarded).
class WsConnection : public std::enable_shared_from_this<WsConnection> {
public:
    explicit WsConnection(asio::ip::tcp::socket socket);

    /// Reads and responds to the HTTP upgrade handshake, then starts the
    /// frame read loop. Calls `on_ready` once the connection is usable for
    /// `send_text`, or leaves it uncalled if the handshake fails.
    void start(std::function<void(std::shared_ptr<WsConnection>)> on_ready,
               std::function<void(std::shared_ptr<WsConnection>)> on_closed);

    /// Sends an unmasked server->client text frame. Safe to call from any
    /// thread that shares the connection's io_context.
    void send_text(const std::string& payload);

    [[nodiscard]] bool is_open() const noexcept {
        return open_;
    }

private:
    void read_handshake();
    void handle_handshake_data(std::size_t bytes_transferred);
    void read_frame_header();
    void handle_frame(std::vector<std::uint8_t> payload, std::uint8_t opcode);
    void close();

    asio::ip::tcp::socket socket_;
    asio::streambuf handshake_buffer_;
    std::array<std::uint8_t, 14> frame_header_buffer_{};
    bool open_ = false;
    std::function<void(std::shared_ptr<WsConnection>)> on_closed_;
};

/// Minimal WebSocket server: accepts connections on a dedicated TCP port
/// and lets the caller broadcast JSON text frames to every connected
/// client (used to push live sensor readings and anomaly alerts to the
/// dashboard — see web/js/ws-client.js).
///
/// Runs on the same io_context as the rest of ironpulse (Modbus polling,
/// REST server thread excluded), so no extra threads are spun up for it.
class WsServer {
public:
    WsServer(asio::io_context& io_context, std::uint16_t port);

    void start();
    void broadcast(const std::string& message);
    [[nodiscard]] std::size_t connection_count() const;

private:
    void do_accept();

    asio::io_context& io_context_;
    asio::ip::tcp::acceptor acceptor_;
    mutable std::mutex connections_mutex_;
    std::vector<std::shared_ptr<WsConnection>> connections_;
};

}  // namespace ironpulse::api
