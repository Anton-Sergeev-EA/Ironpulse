#pragma once

#include <array>
#include <asio.hpp>
#include <atomic>
#include <cstdint>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace ironpulse::api {

/// A single accepted WebSocket connection. Handles the RFC 6455 opening
/// handshake, then supports server->client text frames (broadcast) and
/// reads client frames only to answer ping/close (this server never
/// expects meaningful data *from* the dashboard, so incoming text frames
/// are simply discarded).
///
/// All socket work runs on the connection's strand, and outgoing frames go
/// through a queue with a single write in flight: broadcasts arrive from
/// several polling threads at once, and overlapping async_write calls on
/// one socket would interleave frames and corrupt the stream.
class WsConnection : public std::enable_shared_from_this<WsConnection> {
public:
    /// `required_token`, when non-empty, must match the `token` query
    /// parameter of the upgrade request (browsers cannot set headers on a
    /// WebSocket handshake).
    WsConnection(asio::ip::tcp::socket socket, std::string required_token);

    /// Reads and responds to the HTTP upgrade handshake, then starts the
    /// frame read loop. Calls `on_ready` once the connection is usable for
    /// `send_text`, or leaves it uncalled if the handshake fails.
    void start(std::function<void(std::shared_ptr<WsConnection>)> on_ready,
               std::function<void(std::shared_ptr<WsConnection>)> on_closed);

    /// Queues an unmasked server->client text frame. Safe to call from any
    /// thread.
    void send_text(const std::string& payload);

    [[nodiscard]] bool is_open() const noexcept {
        return open_;
    }

    /// Frames a client may send us at most (we only expect control frames).
    static constexpr std::uint64_t kMaxIncomingPayload = 64 * 1024;
    /// A client this far behind is too slow to keep up with live data and
    /// is disconnected instead of buffering without bound.
    static constexpr std::size_t kMaxQueuedFrames = 512;

private:
    void read_frame_header();
    void handle_frame(const std::vector<std::uint8_t>& payload, std::uint8_t opcode);
    void enqueue(std::shared_ptr<std::vector<std::uint8_t>> frame);
    void write_next();
    void close();
    void reject(const std::string& status_line);

    asio::ip::tcp::socket socket_;
    asio::any_io_executor executor_;  // a strand: the socket was accepted onto one
    std::string required_token_;
    asio::streambuf handshake_buffer_{8192};  // bounded: no unlimited header growth
    std::array<std::uint8_t, 14> frame_header_buffer_{};
    std::deque<std::shared_ptr<std::vector<std::uint8_t>>> outbox_;
    bool writing_ = false;
    std::atomic<bool> open_{false};
    bool closed_ = false;
    std::function<void(std::shared_ptr<WsConnection>)> on_closed_;
};

/// Minimal WebSocket server: accepts connections on a dedicated TCP port
/// and lets the caller broadcast JSON text frames to every connected
/// client (used to push live sensor readings and alerts to the dashboard
/// — see web/js/ws-client.js).
///
/// Runs on the same io_context as the rest of ironpulse, so no extra
/// threads are spun up for it.
class WsServer {
public:
    WsServer(asio::io_context& io_context, std::uint16_t port, std::string required_token = {});

    void start();
    void stop();
    void broadcast(const std::string& message);
    [[nodiscard]] std::size_t connection_count() const;
    [[nodiscard]] std::uint16_t port() const {
        return acceptor_.local_endpoint().port();
    }

private:
    void do_accept();

    asio::io_context& io_context_;
    asio::ip::tcp::acceptor acceptor_;
    std::string required_token_;
    mutable std::mutex connections_mutex_;
    std::vector<std::shared_ptr<WsConnection>> connections_;
};

/// Extracts the value of a query parameter from an HTTP request target
/// such as "/live?token=abc&x=1" (percent-decoding included). Empty if absent.
[[nodiscard]] std::string query_param(const std::string& target, const std::string& name);

}  // namespace ironpulse::api
