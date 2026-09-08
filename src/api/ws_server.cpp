#include "ironpulse/api/ws_server.hpp"

#include <algorithm>

#include "ironpulse/core/base64.hpp"
#include "ironpulse/core/logger.hpp"
#include "ironpulse/core/sha1.hpp"

namespace ironpulse::api {

namespace {
constexpr const char* kWebSocketGuid = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";

std::string compute_accept_key(const std::string& client_key) {
    auto digest = ironpulse::core::Sha1::digest(client_key + kWebSocketGuid);
    return ironpulse::core::Base64::encode(digest.data(), digest.size());
}

/// Extracts the value of a header (case-sensitive on the name we look
/// for, which is fine since we control exactly what we search for) from a
/// raw HTTP request string.
std::string extract_header(const std::string& request, const std::string& header_name) {
    auto pos = request.find(header_name);
    if (pos == std::string::npos) {
        return {};
    }
    pos += header_name.size();
    auto end = request.find("\r\n", pos);
    std::string value = request.substr(pos, end - pos);
    // trim leading spaces
    auto first_non_space = value.find_first_not_of(' ');
    return first_non_space == std::string::npos ? "" : value.substr(first_non_space);
}
}  // namespace

// ---------------------------------------------------------------------------
// WsConnection
// ---------------------------------------------------------------------------

WsConnection::WsConnection(asio::ip::tcp::socket socket) : socket_(std::move(socket)) {}

void WsConnection::start(std::function<void(std::shared_ptr<WsConnection>)> on_ready,
                         std::function<void(std::shared_ptr<WsConnection>)> on_closed) {
    on_closed_ = std::move(on_closed);
    auto self = shared_from_this();

    asio::async_read_until(
        socket_,
        handshake_buffer_,
        "\r\n\r\n",
        [this, self, on_ready = std::move(on_ready)](const asio::error_code& ec, std::size_t /*bytes*/) {
            if (ec) {
                close();
                return;
            }

            // Extract the buffered handshake bytes directly via Asio's
            // buffer iterators rather than std::istreambuf_iterator: the
            // latter triggers a spurious -Wnull-dereference under GCC at
            // -O2/-O3 (see gcc.gnu.org/PR96003) when inlined into Asio's
            // internal read_until state machine — functionally harmless,
            // but this project holds a zero-warnings bar even in Release.
            const auto data = handshake_buffer_.data();
            std::string request(asio::buffers_begin(data), asio::buffers_end(data));
            handshake_buffer_.consume(handshake_buffer_.size());

            const std::string client_key = extract_header(request, "Sec-WebSocket-Key:");
            if (client_key.empty()) {
                IP_LOG_WARN("WS handshake missing Sec-WebSocket-Key, rejecting connection");
                close();
                return;
            }

            const std::string accept_key = compute_accept_key(client_key);
            const std::string response =
                "HTTP/1.1 101 Switching Protocols\r\n"
                "Upgrade: websocket\r\n"
                "Connection: Upgrade\r\n"
                "Sec-WebSocket-Accept: " +
                accept_key + "\r\n\r\n";

            auto response_buf = std::make_shared<std::string>(response);
            asio::async_write(
                socket_,
                asio::buffer(*response_buf),
                [this, self, response_buf, on_ready](const asio::error_code& write_ec, std::size_t) {
                    if (write_ec) {
                        close();
                        return;
                    }
                    open_ = true;
                    on_ready(self);
                    read_frame_header();
                });
        });
}

void WsConnection::read_frame_header() {
    auto self = shared_from_this();
    asio::async_read(
        socket_,
        asio::buffer(frame_header_buffer_.data(), 2),
        [this, self](const asio::error_code& ec, std::size_t) {
            if (ec) {
                close();
                return;
            }

            const std::uint8_t byte0 = frame_header_buffer_[0];
            const std::uint8_t byte1 = frame_header_buffer_[1];
            const std::uint8_t opcode = byte0 & 0x0F;
            const bool masked = (byte1 & 0x80) != 0;
            std::uint64_t payload_len = byte1 & 0x7F;

            // Extended length / mask key handling done inline (small
            // reads chained) to keep this a self-contained minimal
            // implementation without a general-purpose frame parser.
            auto finish_with_length = [this, self, opcode, masked](std::uint64_t len) {
                if (!masked) {
                    // Per RFC 6455, client->server frames MUST be masked;
                    // treat an unmasked frame as a protocol violation.
                    close();
                    return;
                }
                auto mask_key = std::make_shared<std::array<std::uint8_t, 4>>();
                asio::async_read(
                    socket_,
                    asio::buffer(*mask_key),
                    [this, self, mask_key, len, opcode](const asio::error_code& mask_ec, std::size_t) {
                        if (mask_ec) {
                            close();
                            return;
                        }
                        auto payload = std::make_shared<std::vector<std::uint8_t>>(len);
                        if (len == 0) {
                            handle_frame(*payload, opcode);
                            read_frame_header();
                            return;
                        }
                        asio::async_read(socket_,
                                         asio::buffer(*payload),
                                         [this, self, payload, mask_key, opcode](
                                             const asio::error_code& payload_ec, std::size_t) {
                                             if (payload_ec) {
                                                 close();
                                                 return;
                                             }
                                             for (std::size_t i = 0; i < payload->size(); ++i) {
                                                 (*payload)[i] = static_cast<std::uint8_t>(
                                                     (*payload)[i] ^ (*mask_key)[i % 4]);
                                             }
                                             handle_frame(*payload, opcode);
                                             read_frame_header();
                                         });
                    });
            };

            if (payload_len == 126) {
                auto ext = std::make_shared<std::array<std::uint8_t, 2>>();
                asio::async_read(
                    socket_,
                    asio::buffer(*ext),
                    [this, self, ext, finish_with_length](const asio::error_code& ext_ec, std::size_t) {
                        if (ext_ec) {
                            close();
                            return;
                        }
                        std::uint64_t len = (static_cast<std::uint64_t>((*ext)[0]) << 8) | (*ext)[1];
                        finish_with_length(len);
                    });
            } else if (payload_len == 127) {
                auto ext = std::make_shared<std::array<std::uint8_t, 8>>();
                asio::async_read(
                    socket_,
                    asio::buffer(*ext),
                    [this, self, ext, finish_with_length](const asio::error_code& ext_ec, std::size_t) {
                        if (ext_ec) {
                            close();
                            return;
                        }
                        std::uint64_t len = 0;
                        for (int i = 0; i < 8; ++i) {
                            len = (len << 8) | (*ext)[static_cast<std::size_t>(i)];
                        }
                        finish_with_length(len);
                    });
            } else {
                finish_with_length(payload_len);
            }
        });
}

void WsConnection::handle_frame(std::vector<std::uint8_t> payload, std::uint8_t opcode) {
    constexpr std::uint8_t kOpClose = 0x8;
    constexpr std::uint8_t kOpPing = 0x9;

    if (opcode == kOpClose) {
        close();
        return;
    }
    if (opcode == kOpPing) {
        // Reply with a pong carrying the same payload (RFC 6455 §5.5.3).
        std::vector<std::uint8_t> frame;
        frame.push_back(0x8A);  // FIN + pong opcode
        frame.push_back(static_cast<std::uint8_t>(payload.size() & 0x7F));
        frame.insert(frame.end(), payload.begin(), payload.end());
        auto buf = std::make_shared<std::vector<std::uint8_t>>(std::move(frame));
        auto self = shared_from_this();
        asio::async_write(socket_, asio::buffer(*buf), [self, buf](const asio::error_code&, std::size_t) {});
    }
    // Text/binary frames from the client are intentionally ignored — this
    // server is push-only from the dashboard's perspective.
}

void WsConnection::send_text(const std::string& payload) {
    if (!open_) {
        return;
    }

    std::vector<std::uint8_t> frame;
    frame.push_back(0x81);  // FIN + text opcode

    const std::size_t len = payload.size();
    if (len <= 125) {
        frame.push_back(static_cast<std::uint8_t>(len));
    } else if (len <= 0xFFFF) {
        frame.push_back(126);
        frame.push_back(static_cast<std::uint8_t>((len >> 8) & 0xFF));
        frame.push_back(static_cast<std::uint8_t>(len & 0xFF));
    } else {
        frame.push_back(127);
        for (int i = 7; i >= 0; --i) {
            frame.push_back(static_cast<std::uint8_t>((static_cast<std::uint64_t>(len) >> (i * 8)) & 0xFF));
        }
    }

    frame.insert(frame.end(), payload.begin(), payload.end());

    auto buf = std::make_shared<std::vector<std::uint8_t>>(std::move(frame));
    auto self = shared_from_this();
    asio::async_write(socket_, asio::buffer(*buf), [self, buf](const asio::error_code& ec, std::size_t) {
        if (ec) {
            self->close();
        }
    });
}

void WsConnection::close() {
    if (!open_) {
        return;
    }
    open_ = false;
    asio::error_code ec;
    socket_.shutdown(asio::ip::tcp::socket::shutdown_both, ec);
    socket_.close(ec);
    if (on_closed_) {
        on_closed_(shared_from_this());
    }
}

// ---------------------------------------------------------------------------
// WsServer
// ---------------------------------------------------------------------------

WsServer::WsServer(asio::io_context& io_context, std::uint16_t port)
    : io_context_(io_context), acceptor_(io_context, asio::ip::tcp::endpoint(asio::ip::tcp::v4(), port)) {}

void WsServer::start() {
    IP_LOG_INFO("WebSocket server listening on port {}", acceptor_.local_endpoint().port());
    do_accept();
}

void WsServer::do_accept() {
    acceptor_.async_accept([this](const asio::error_code& ec, asio::ip::tcp::socket socket) {
        if (!ec) {
            auto connection = std::make_shared<WsConnection>(std::move(socket));
            connection->start(
                [this](std::shared_ptr<WsConnection> conn) {
                    std::lock_guard lock(connections_mutex_);
                    connections_.push_back(std::move(conn));
                },
                [this](const std::shared_ptr<WsConnection>& conn) {
                    std::lock_guard lock(connections_mutex_);
                    connections_.erase(std::remove(connections_.begin(), connections_.end(), conn),
                                       connections_.end());
                });
        }
        do_accept();
    });
}

void WsServer::broadcast(const std::string& message) {
    std::lock_guard lock(connections_mutex_);
    for (auto& conn : connections_) {
        if (conn->is_open()) {
            conn->send_text(message);
        }
    }
}

std::size_t WsServer::connection_count() const {
    std::lock_guard lock(connections_mutex_);
    return connections_.size();
}

}  // namespace ironpulse::api
