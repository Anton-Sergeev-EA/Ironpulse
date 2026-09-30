#include "ironpulse/notify/notifier.hpp"

#include <httplib.h>

#include <ctime>
#include <nlohmann/json.hpp>

#include "ironpulse/api/serialization.hpp"
#include "ironpulse/core/logger.hpp"

namespace ironpulse::notify {

namespace {

std::string iso8601(std::chrono::system_clock::time_point tp) {
    const std::time_t t = std::chrono::system_clock::to_time_t(tp);
    std::tm tm{};
#if defined(_WIN32)
    gmtime_s(&tm, &t);
#else
    gmtime_r(&t, &tm);
#endif
    char buffer[32];
    std::strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%SZ", &tm);
    return buffer;
}

const char* type_name(Notification::Type type) {
    switch (type) {
        case Notification::Type::alert:
            return "alert";
        case Notification::Type::device_offline:
            return "device_offline";
        case Notification::Type::device_online:
            return "device_online";
    }
    return "alert";
}

}  // namespace

std::optional<Url> parse_url(const std::string& text) {
    Url url;
    const auto scheme_end = text.find("://");
    if (scheme_end == std::string::npos) {
        return std::nullopt;
    }
    url.scheme = text.substr(0, scheme_end);
    if (url.scheme != "http" && url.scheme != "https") {
        return std::nullopt;
    }
    const auto authority_start = scheme_end + 3;
    const auto path_start = text.find('/', authority_start);
    const std::string authority = text.substr(
        authority_start, path_start == std::string::npos ? std::string::npos : path_start - authority_start);
    url.path = path_start == std::string::npos ? "/" : text.substr(path_start);
    if (authority.empty() || authority.find('@') != std::string::npos) {
        return std::nullopt;  // credentials in URLs are not supported
    }

    std::string port_text;
    if (authority.front() == '[') {  // IPv6 literal
        const auto close = authority.find(']');
        if (close == std::string::npos) {
            return std::nullopt;
        }
        url.host = authority.substr(1, close - 1);
        if (close + 1 < authority.size()) {
            if (authority[close + 1] != ':') {
                return std::nullopt;
            }
            port_text = authority.substr(close + 2);
        }
    } else {
        const auto colon = authority.rfind(':');
        url.host = authority.substr(0, colon);
        if (colon != std::string::npos) {
            port_text = authority.substr(colon + 1);
        }
    }
    if (url.host.empty()) {
        return std::nullopt;
    }

    if (port_text.empty()) {
        url.port = url.scheme == "https" ? 443 : 80;
    } else {
        try {
            std::size_t consumed = 0;
            const unsigned long port = std::stoul(port_text, &consumed);
            if (consumed != port_text.size() || port == 0 || port > 65535) {
                return std::nullopt;
            }
            url.port = static_cast<std::uint16_t>(port);
        } catch (const std::exception&) {
            return std::nullopt;
        }
    }
    return url;
}

OutgoingRequest build_request(const core::NotificationChannelConfig& channel,
                              const Notification& notification,
                              const std::string& language,
                              const std::string& dashboard_url) {
    OutgoingRequest request;

    if (channel.type == "telegram") {
        // `url` optionally points at a self-hosted Bot API server or proxy
        // (useful where api.telegram.org is unreachable).
        std::string base = channel.url.empty() ? "https://api.telegram.org" : channel.url;
        while (!base.empty() && base.back() == '/') {
            base.pop_back();
        }
        request.url = base + "/bot" + channel.bot_token + "/sendMessage";
        // Plain text, no parse_mode: sensor names may contain characters
        // Telegram's Markdown/HTML modes would reject.
        request.body =
            nlohmann::json{
                {"chat_id", channel.chat_id},
                {"text", notification.text},
                {"disable_web_page_preview", true},
            }
                .dump();
        return request;
    }

    if (channel.type == "slack") {
        request.url = channel.url;
        request.body = nlohmann::json{{"text", notification.text}}.dump();
        return request;
    }

    // Generic webhook: a stable, structured payload for integrations
    // (ticketing, PagerDuty-style relays, custom automation).
    nlohmann::json body{
        {"type", type_name(notification.type)},
        {"severity", core::to_string(notification.severity)},
        {"title", notification.title},
        {"text", notification.text},
        {"language", language},
        {"timestamp", iso8601(notification.timestamp)},
        {"device_id", notification.device_id},
    };
    if (!dashboard_url.empty()) {
        body["dashboard_url"] = dashboard_url;
    }
    if (notification.alert) {
        nlohmann::json alert = api::alert_fields(*notification.alert);
        alert["timestamp"] = iso8601(notification.alert->timestamp);
        body["sensor_id"] = notification.sensor_id;
        body["alert"] = std::move(alert);
    }
    request.url = channel.url;
    request.headers = channel.headers;
    request.body = body.dump();
    return request;
}

Notifier::Notifier(core::NotificationsConfig config,
                   std::map<std::string, SensorInfo> sensors,
                   ResultCallback on_result,
                   Transport transport)
    : config_(std::move(config)),
      sensors_(std::move(sensors)),
      on_result_(std::move(on_result)),
      transport_(transport ? std::move(transport) : Transport(&Notifier::http_post)) {}

Notifier::~Notifier() {
    stop(std::chrono::milliseconds(0));
}

void Notifier::start() {
    if (!enabled() || worker_.joinable()) {
        return;
    }
    for (const auto& channel : config_.channels) {
        IP_LOG_INFO("Notifications enabled: {} (language: {})", channel.type, config_.language);
    }
    worker_ = std::thread([this] { run(); });
}

void Notifier::stop(std::chrono::milliseconds drain_timeout) {
    {
        std::lock_guard lock(mutex_);
        if (stopping_) {
            return;
        }
        stopping_ = true;
        drain_deadline_ = std::chrono::steady_clock::now() + drain_timeout;
    }
    cv_.notify_all();
    if (worker_.joinable()) {
        worker_.join();
    }
}

void Notifier::on_alert(const core::AnomalyEvent& alert) {
    if (!enabled() || alert.severity < config_.min_severity) {
        return;
    }
    SensorInfo info;
    if (auto it = sensors_.find(alert.sensor_id); it != sensors_.end()) {
        info = it->second;
    }
    enqueue(describe_alert(alert, info, config_.language, config_.dashboard_url));
}

void Notifier::on_device_status(const std::string& device_id,
                                bool online,
                                std::chrono::system_clock::time_point timestamp) {
    if (!enabled()) {
        return;
    }
    {
        // "Back online" is only worth saying if we said it went offline;
        // a device coming up normally at startup is not news.
        std::lock_guard lock(status_mutex_);
        bool& was_reported_offline = notified_offline_[device_id];
        if (online && !was_reported_offline) {
            return;
        }
        if (!online && was_reported_offline) {
            return;
        }
        was_reported_offline = !online;
    }
    enqueue(describe_device_status(device_id, online, timestamp, config_.language, config_.dashboard_url));
}

void Notifier::enqueue(Notification notification) {
    {
        std::lock_guard lock(mutex_);
        if (stopping_) {
            return;
        }
        if (queue_.size() >= kMaxQueue) {
            queue_.pop_front();
            IP_LOG_WARN("Notification queue full ({}), dropping the oldest notification", kMaxQueue);
        }
        queue_.push_back(std::move(notification));
    }
    cv_.notify_one();
}

void Notifier::run() {
    for (;;) {
        Notification next;
        {
            std::unique_lock lock(mutex_);
            cv_.wait(lock, [this] { return stopping_ || !queue_.empty(); });
            if (queue_.empty() || (stopping_ && std::chrono::steady_clock::now() >= drain_deadline_)) {
                return;
            }
            next = std::move(queue_.front());
            queue_.pop_front();
        }
        for (const auto& channel : config_.channels) {
            const bool ok = deliver(channel, next);
            if (on_result_) {
                on_result_(channel.type, ok);
            }
        }
    }
}

bool Notifier::deliver(const core::NotificationChannelConfig& channel, const Notification& notification) {
    const OutgoingRequest request =
        build_request(channel, notification, config_.language, config_.dashboard_url);
    auto backoff = std::chrono::seconds(1);
    int status = 0;
    for (int attempt = 1; attempt <= kMaxAttempts; ++attempt) {
        status = transport_(request);
        if (status >= 200 && status < 300) {
            return true;
        }
        // A 4xx other than 429 means the request itself is wrong (bad
        // token, unknown chat): retrying cannot help.
        if (status >= 400 && status < 500 && status != 429) {
            break;
        }
        if (attempt == kMaxAttempts) {
            break;
        }
        std::unique_lock lock(mutex_);
        if (cv_.wait_for(lock, backoff, [this] {
                return stopping_ && std::chrono::steady_clock::now() >= drain_deadline_;
            })) {
            break;
        }
        backoff *= 2;
    }
    // Never log the URL: for Telegram it contains the bot token.
    IP_LOG_WARN("Notification via {} failed ({})",
                channel.type,
                status == 0 ? std::string("network error") : "HTTP " + std::to_string(status));
    return false;
}

int Notifier::http_post(const OutgoingRequest& request) {
    const auto url = parse_url(request.url);
    if (!url) {
        return 0;
    }
#if !defined(CPPHTTPLIB_OPENSSL_SUPPORT)
    if (url->scheme == "https") {
        IP_LOG_ERROR("This build has no TLS support; cannot deliver to an https:// endpoint");
        return 0;
    }
#endif
    const bool ipv6 = url->host.find(':') != std::string::npos;
    const std::string origin =
        url->scheme + "://" + (ipv6 ? "[" + url->host + "]" : url->host) + ":" + std::to_string(url->port);
    httplib::Client client(origin);
    if (!client.is_valid()) {
        return 0;
    }
    client.set_connection_timeout(std::chrono::seconds(10));
    client.set_read_timeout(std::chrono::seconds(10));
    client.set_write_timeout(std::chrono::seconds(10));

    httplib::Headers headers;
    for (const auto& [name, value] : request.headers) {
        headers.emplace(name, value);
    }
    auto result = client.Post(url->path, headers, request.body, request.content_type);
    return result ? result->status : 0;
}

}  // namespace ironpulse::notify
