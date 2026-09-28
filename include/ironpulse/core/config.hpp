#pragma once

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include "ironpulse/core/events.hpp"

namespace ironpulse::core {

/// Which Modbus table a sensor lives in.
enum class RegisterType {
    holding,  // function code 0x03, read/write registers
    input,    // function code 0x04, read-only registers
};

/// How the raw 16-bit register(s) are interpreted.
enum class DataType { uint16, int16, uint32, int32, float32 };

/// For 32-bit values spanning two registers: which register holds the
/// high word. `big` (high word first, "ABCD") is the Modbus convention;
/// many PLCs and meters use `little` ("CDAB") instead.
enum class WordOrder { big, little };

/// Number of 16-bit registers a value of this type occupies.
[[nodiscard]] constexpr std::uint16_t register_count(DataType type) noexcept {
    return (type == DataType::uint16 || type == DataType::int16) ? 1 : 2;
}

/// One anomaly-detection strategy and its tuning. Which fields apply
/// depends on `type`: "zscore" (window, threshold), "ewma" (alpha,
/// threshold) or "cusum" (mean, stddev, slack, threshold).
struct DetectorConfig {
    std::string type;
    std::size_t window = 60;
    double alpha = 0.2;
    double threshold = 3.0;
    double mean = 0.0;
    double stddev = 1.0;
    double slack = 0.5;
};

struct DetectionConfig {
    /// Empty disables statistical detection (limits still apply).
    std::vector<DetectorConfig> detectors;
    /// How many detectors must agree on a reading before an alert is raised.
    std::size_t votes_required = 1;
    /// Minimum time between two alerts of the same kind for one sensor, so
    /// a sustained excursion produces one alert rather than one per sample.
    std::uint32_t cooldown_seconds = 30;
};

/// Hard engineering limits. Crossing one raises a critical alert.
struct Limits {
    std::optional<double> low;
    std::optional<double> high;
};

/// One measured quantity on a device, e.g. "winding temperature".
struct SensorConfig {
    std::string id;    // unique across all devices; also the storage file name
    std::string name;  // human-readable label for the dashboard and notifications
    std::string unit;  // e.g. "°C", "mm/s", "bar"
    RegisterType register_type = RegisterType::holding;
    std::uint16_t address = 0;
    DataType data_type = DataType::uint16;
    WordOrder word_order = WordOrder::big;
    /// Engineering value = raw * scale + offset.
    double scale = 1.0;
    double offset = 0.0;
    Limits limits;
    DetectionConfig detection;
};

struct ModbusDeviceConfig {
    std::string id;
    std::string host;
    std::uint16_t port = 502;
    std::uint8_t unit_id = 1;
    std::uint32_t poll_interval_ms = 1000;
    /// A request without a response within this time marks the device
    /// offline and forces a reconnect.
    std::uint32_t timeout_ms = 3000;
    std::vector<SensorConfig> sensors;
};

/// Where alerts are delivered besides the dashboard.
struct NotificationChannelConfig {
    std::string type;  // "telegram", "slack" or "webhook"
    std::string url;   // slack, webhook
    std::string bot_token;
    std::string chat_id;                         // telegram
    std::map<std::string, std::string> headers;  // webhook
};

struct NotificationsConfig {
    /// Language of notification texts: ru, en, zh, hi, es, fr, de, it.
    std::string language = "ru";
    Severity min_severity = Severity::warning;
    /// Optional public URL of the dashboard, linked from notifications.
    std::string dashboard_url;
    std::vector<NotificationChannelConfig> channels;
};

struct AppConfig {
    std::string app_name = "ironpulse";
    std::string log_level = "info";
    std::string log_file;
    std::uint16_t http_port = 8080;
    std::uint16_t ws_port = 8081;
    std::size_t worker_threads = 4;
    std::string web_root = "web";
    std::string data_dir = "data";
    bool persistence_enabled = false;
    std::uint32_t retention_hours = 168;  // 7 days
    /// When non-empty, the REST API, WebSocket and /metrics require this
    /// token (Authorization: Bearer <token>). /healthz stays open.
    std::string api_token;
    std::vector<ModbusDeviceConfig> devices;
    NotificationsConfig notifications;

    /// Non-fatal problems found while loading (e.g. a notification channel
    /// left without credentials); the caller logs them at startup.
    std::vector<std::string> warnings;

    /// Loads configuration from a JSON file, applies IRONPULSE_* environment
    /// overrides and validates the result. Throws std::runtime_error with a
    /// human-readable message on a missing file, malformed JSON or invalid
    /// settings.
    static AppConfig load_from_file(const std::string& path);

    /// Same as load_from_file, but from an in-memory JSON document.
    static AppConfig load_from_string(const std::string& json_text);
};

/// Replaces every ${NAME} in `text` with the value of environment variable
/// NAME (empty if unset). Lets secrets stay out of checked-in config files.
[[nodiscard]] std::string expand_env(const std::string& text);

/// True for identifiers safe to use as file names and URL path segments:
/// 1-64 characters from [A-Za-z0-9_.-], not starting with a dot.
[[nodiscard]] bool is_valid_id(const std::string& id);

[[nodiscard]] bool is_supported_language(const std::string& code);

}  // namespace ironpulse::core
