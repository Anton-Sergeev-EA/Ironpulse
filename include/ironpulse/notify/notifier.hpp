#pragma once

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <deque>
#include <functional>
#include <map>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>

#include "ironpulse/core/config.hpp"
#include "ironpulse/notify/messages.hpp"

namespace ironpulse::notify {

/// A parsed http(s) URL.
struct Url {
    std::string scheme;  // "http" or "https"
    std::string host;
    std::uint16_t port = 0;
    std::string path;  // includes the query string; "/" if empty
};

[[nodiscard]] std::optional<Url> parse_url(const std::string& text);

/// A ready-to-send HTTP POST, built from a channel and a notification.
struct OutgoingRequest {
    std::string url;
    std::string body;
    std::string content_type = "application/json";
    std::map<std::string, std::string> headers;
};

/// Builds the request a channel sends for a notification. Pure, so each
/// channel's payload format is unit-tested without a network.
///   telegram -> Bot API sendMessage
///   slack    -> incoming-webhook {"text": ...}
///   webhook  -> structured JSON for integrations (see README)
[[nodiscard]] OutgoingRequest build_request(const core::NotificationChannelConfig& channel,
                                            const Notification& notification,
                                            const std::string& language,
                                            const std::string& dashboard_url);

/// Delivers notifications to the configured channels on a background
/// thread, so a slow or unreachable chat service never blocks polling or
/// alerting. Each delivery is retried with exponential backoff; the queue
/// is bounded and drops the oldest entries under a sustained flood.
class Notifier {
public:
    /// Called after every delivery attempt finishes (after retries):
    /// channel type and whether it succeeded. Used for metrics.
    using ResultCallback = std::function<void(const std::string& channel, bool ok)>;

    /// Signature of the transport, injectable for tests. Returns the HTTP
    /// status code, or 0 on a network error.
    using Transport = std::function<int(const OutgoingRequest&)>;

    Notifier(core::NotificationsConfig config,
             std::map<std::string, SensorInfo> sensors,
             ResultCallback on_result = {},
             Transport transport = {});
    ~Notifier();

    Notifier(const Notifier&) = delete;
    Notifier& operator=(const Notifier&) = delete;

    void start();
    /// Stops the worker after delivering what is already queued, waiting
    /// at most `drain_timeout`.
    void stop(std::chrono::milliseconds drain_timeout = std::chrono::seconds(5));

    void on_alert(const core::AnomalyEvent& alert);
    void on_device_status(const std::string& device_id,
                          bool online,
                          std::chrono::system_clock::time_point timestamp);

    [[nodiscard]] bool enabled() const noexcept {
        return !config_.channels.empty();
    }

    static constexpr std::size_t kMaxQueue = 1000;
    static constexpr int kMaxAttempts = 3;

    /// Default transport: cpp-httplib client with 10 s timeouts.
    static int http_post(const OutgoingRequest& request);

private:
    void enqueue(Notification notification);
    void run();
    bool deliver(const core::NotificationChannelConfig& channel, const Notification& notification);

    core::NotificationsConfig config_;
    std::map<std::string, SensorInfo> sensors_;
    ResultCallback on_result_;
    Transport transport_;

    std::mutex mutex_;
    std::condition_variable cv_;
    std::deque<Notification> queue_;
    bool stopping_ = false;
    std::chrono::steady_clock::time_point drain_deadline_{};
    std::thread worker_;

    std::mutex status_mutex_;
    std::map<std::string, bool> notified_offline_;
};

}  // namespace ironpulse::notify
