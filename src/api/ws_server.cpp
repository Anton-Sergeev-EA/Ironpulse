#include "ironpulse/api/ws_server.hpp"

#include <algorithm>
#include <cctype>

#include "ironpulse/api/http_server.hpp"
#include "ironpulse/core/base64.hpp"
#include "ironpulse/core/logger.hpp"
#include "ironpulse/core/sha1.hpp"

namespace ironpulse::api {

namespace {
constexpr const char* kWebSocketGuid = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";

constexpr std::uint8_t kOpText = 0x1;
constexpr std::uint8_t kOpClose = 0x8;
constexpr std::uint8_t kOpPing = 0x9;
constexpr std::uint8_t kOpPong = 0xA;

std::string compute_accept_key(const std::string& client_key) {
    auto digest = ironpulse::core::Sha1::digest(client_key + kWebSocketGuid);
    return ironpulse::core::Base64::encode(digest.data(), digest.size());
}

std::string to_lower(std::string s) {
    std::transform(
        s.begin(), s.end(), s.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return s;
}

/// Extracts a header value from a raw HTTP request. Header names are
/// case-insensitive (RFC 9110), and proxies do lowercase them.
std::string extract_header(const std::string& request, const std::string& header_name) {
    const std::string lower = to_lower(request);
    const std::string needle = "\r\n" + to_lower(header_name) + ":";
    auto pos = lower.find(needle);
    if (pos == std::string::npos) {
        return {};
    }
    pos += needle.size();
    const auto end = request.find("\r\n", pos);
    std::string value = request.substr(pos, end - pos);
    const auto first = value.find_first_not_of(" \t");
    const auto last = value.find_last_not_of(" \t");
    return first == std::string::npos ? "" : value.substr(first, last - first + 1);
}

std::shared_ptr<std::vector<std::uint8_t>> make_frame(std::uint8_t opcode,
                                                      const std::uint8_t* data,
                                                      std::size_t len) {
    auto frame = std::make_shared<std::vector<std::uint8_t>>();
    frame->reserve(len + 10);
    frame->push_back(static_cast<std::uint8_t>(0x80 | opcode));  // FIN + opcode
    if (len <= 125) {
        frame->push_back(static_cast<std::uint8_t>(len));
    } else if (len <= 0xFFFF) {
        frame->push_back(126);
        frame->push_back(static_cast<std::uint8_t>((len >> 8) & 0xFF));
        frame->push_back(static_cast<std::uint8_t>(len & 0xFF));
    } else {
        frame->push_back(127);
        for (int i = 7; i >= 0; --i) {
            frame->push_back(static_cast<std::uint8_t>((static_cast<std::uint64_t>(len) >> (i * 8)) & 0xFF));
        }
    }
    frame->insert(frame->end(), data, data + len);
    return frame;
}

int hex_value(char c) {
    if (c >= '0' && c <= '9')
        return c - '0';
    if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;
    if (c >= 'A' && c <= 'F')
        return c - 'A' + 10;
    return -1;
}

std::string percent_decode(const std::string& in) {
    std::string out;
    out.reserve(in.size());
    for (std::size_t i = 0; i < in.size(); ++i) {
        if (in[i] == '%' && i + 2 < in.size() && hex_value(in[i + 1]) >= 0 && hex_value(in[i + 2]) >= 0) {
            out += static_cast<char>(hex_value(in[i + 1]) * 16 + hex_value(in[i + 2]));
            i += 2;
        } else if (in[i] == '+') {
            out += ' ';
        } else {
            out += in[i];
        }
    }
    return out;
}
}  // namespace

std::string query_param(const std::string& target, const std::string& name) {
    const auto q = target.find('?');
    if (q == std::string::npos) {
        return {};
    }
    std::size_t pos = q + 1;
    while (pos <= target.size()) {
        auto end = target.find('&', pos);
        if (end == std::string::npos) {
            end = target.size();
        }
        const std::string pair = target.substr(pos, end - pos);
        const auto eq = pair.find('=');
        const std::string key = percent_decode(pair.substr(0, eq));
        if (key == name) {
            return eq == std::string::npos ? "" : percent_decode(pair.substr(eq + 1));
        }
        pos = end + 1;
    }
    return {};
}

// ---------------------------------------------------------------------------
// WsConnection
// ---------------------------------------------------------------------------

WsConnection::WsConnection(asio::ip::tcp::socket socket, std::string required_token)
    : socket_(std::move(socket)),
      executor_(socket_.get_executor()),
      required_token_(std::move(required_token)) {}

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
                // Includes asio::error::not_found when the headers exceed
                // the buffer's 8 KiB bound.
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
            const std::string request(asio::buffers_begin(data), asio::buffers_end(data));
            handshake_buffer_.consume(handshake_buffer_.size());

            const auto line_end = request.find("\r\n");
            const std::string request_line = request.substr(0, line_end);
            const auto first_space = request_line.find(' ');
            const auto second_space = request_line.find(' ', first_space + 1);
            const std::string target =
                first_space == std::string::npos
                    ? std::string{}
                    : request_line.substr(first_space + 1, second_space - first_space - 1);

            const std::string client_key = extract_header(request, "Sec-WebSocket-Key");
            if (request_line.rfind("GET ", 0) != 0 || client_key.empty()) {
                reject("400 Bad Request");
                return;
            }
            if (!required_token_.empty() &&
                !constant_time_equals(query_param(target, "token"), required_token_)) {
                reject("401 Unauthorized");
                return;
            }

            auto response = std::make_shared<std::string>(
                "HTTP/1.1 101 Switching Protocols\r\n"
                "Upgrade: websocket\r\n"
                "Connection: Upgrade\r\n"
                "Sec-WebSocket-Accept: " +
                compute_accept_key(client_key) + "\r\n\r\n");

            asio::async_write(
                socket_,
                asio::buffer(*response),
                [this, self, response, on_ready](const asio::error_code& write_ec, std::size_t) {
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

void WsConnection::reject(const std::string& status_line) {
    auto self = shared_from_this();
    auto response = std::make_shared<std::string>("HTTP/1.1 " + status_line +
                                                  "\r\nContent-Length: 0\r\nConnection: close\r\n\r\n");
    asio::async_write(socket_,
                      asio::buffer(*response),
                      [this, self, response](const asio::error_code&, std::size_t) { close(); });
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
            const std::uint64_t payload_len = byte1 & 0x7F;

            // Extended length / mask key handling done inline (small
            // reads chained) to keep this a self-contained minimal
            // implementation without a general-purpose frame parser.
            auto finish_with_length = [this, self, opcode, masked](std::uint64_t len) {
                if (!masked || len > kMaxIncomingPayload) {
                    // Per RFC 6455, client->server frames MUST be masked;
                    // and a dashboard never sends large frames — refusing
                    // them stops a client from making us allocate
                    // arbitrary amounts of memory.
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
                        auto payload =
                            std::make_shared<std::vector<std::uint8_t>>(static_cast<std::size_t>(len));
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

void WsConnection::handle_frame(const std::vector<std::uint8_t>& payload, std::uint8_t opcode) {
    if (opcode == kOpClose) {
        close();
        return;
    }
    if (opcode == kOpPing) {
        // Reply with a pong carrying the same payload (RFC 6455 §5.5.3).
        enqueue(make_frame(kOpPong, payload.data(), std::min<std::size_t>(payload.size(), 125)));
    }
    // Text/binary frames from the client are intentionally ignored — this
    // server is push-only from the dashboard's perspective.
}

void WsConnection::send_text(const std::string& payload) {
    if (!open_) {
        return;
    }
    auto frame = make_frame(kOpText, reinterpret_cast<const std::uint8_t*>(payload.data()), payload.size());
    auto self = shared_from_this();
    asio::post(executor_, [this, self, frame = std::move(frame)]() mutable { enqueue(std::move(frame)); });
}

// Runs on the strand.
void WsConnection::enqueue(std::shared_ptr<std::vector<std::uint8_t>> frame) {
    if (closed_) {
        return;
    }
    if (outbox_.size() >= kMaxQueuedFrames) {
        IP_LOG_WARN("WebSocket client too slow ({} frames queued), disconnecting", outbox_.size());
        close();
        return;
    }
    outbox_.push_back(std::move(frame));
    if (!writing_) {
        write_next();
    }
}

// Runs on the strand.
void WsConnection::write_next() {
    if (outbox_.empty() || closed_) {
        writing_ = false;
        return;
    }
    writing_ = true;
    auto frame = outbox_.front();
    auto self = shared_from_this();
    asio::async_write(
        socket_, asio::buffer(*frame), [this, self, frame](const asio::error_code& ec, std::size_t) {
            if (ec) {
                close();
                return;
            }
            outbox_.pop_front();
            write_next();
        });
}

// Runs on the strand.
void WsConnection::close() {
    if (closed_) {
        return;
    }
    closed_ = true;
    open_ = false;
    outbox_.clear();
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

WsServer::WsServer(asio::io_context& io_context, std::uint16_t port, std::string required_token)
    : io_context_(io_context),
      acceptor_(io_context, asio::ip::tcp::endpoint(asio::ip::tcp::v4(), port)),
      required_token_(std::move(required_token)) {}

void WsServer::start() {
    IP_LOG_INFO("WebSocket server listening on port {}", acceptor_.local_endpoint().port());
    do_accept();
}

void WsServer::stop() {
    asio::post(acceptor_.get_executor(), [this] {
        asio::error_code ec;
        acceptor_.close(ec);
    });
}

void WsServer::do_accept() {
    // Each accepted socket gets its own strand, which serializes all of
    // that connection's handlers.
    acceptor_.async_accept(
        asio::make_strand(io_context_), [this](const asio::error_code& ec, asio::ip::tcp::socket socket) {
            if (ec == asio::error::operation_aborted) {
                return;  // acceptor closed by stop()
            }
            if (!ec) {
                auto connection = std::make_shared<WsConnection>(std::move(socket), required_token_);
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
    std::vector<std::shared_ptr<WsConnection>> targets;
    {
        std::lock_guard lock(connections_mutex_);
        targets = connections_;
    }
    for (auto& conn : targets) {
        conn->send_text(message);
    }
}

std::size_t WsServer::connection_count() const {
    std::lock_guard lock(connections_mutex_);
    return connections_.size();
}

}  // namespace ironpulse::api
