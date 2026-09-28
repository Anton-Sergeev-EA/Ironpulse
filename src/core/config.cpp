#include "ironpulse/core/config.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <limits>
#include <nlohmann/json.hpp>
#include <optional>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <type_traits>

namespace ironpulse::core {

namespace {

using nlohmann::json;

/// Reads an environment variable, returning std::nullopt if unset. Kept
/// as a tiny wrapper so callers don't sprinkle getenv() null-checks
/// everywhere.
std::optional<std::string> env(const char* name) {
    const char* value = std::getenv(name);
    if (value == nullptr || value[0] == '\0') {
        return std::nullopt;
    }
    return std::string(value);
}

template <typename T>
T parse_or(const std::optional<std::string>& raw, T fallback) {
    if (!raw) {
        return fallback;
    }
    try {
        if constexpr (std::is_same_v<T, bool>) {
            return *raw == "1" || *raw == "true" || *raw == "TRUE" || *raw == "yes";
        } else {
            const unsigned long long parsed = std::stoull(*raw);
            if (parsed > std::numeric_limits<T>::max()) {
                return fallback;
            }
            return static_cast<T>(parsed);
        }
    } catch (const std::exception&) {
        return fallback;
    }
}

[[noreturn]] void fail(const std::string& path, const std::string& problem) {
    throw std::runtime_error("config: " + path + ": " + problem);
}

std::string join_path(const std::string& base, const std::string& key) {
    return base.empty() ? key : base + "." + key;
}

/// Reads an integer field with range checking — nlohmann's get<uint16_t>
/// would silently truncate 70000 to 4464, which is the kind of mistake
/// that turns into "why is it polling port 4464?" at 3 a.m.
template <typename T>
T get_int(const json& obj, const char* key, T fallback, const std::string& path, T min_value, T max_value) {
    if (!obj.contains(key)) {
        return fallback;
    }
    const json& v = obj.at(key);
    const std::string where = join_path(path, key);
    if (!v.is_number_integer()) {
        fail(where, "expected an integer");
    }
    const auto raw = v.get<std::int64_t>();
    if (raw < static_cast<std::int64_t>(min_value) || raw > static_cast<std::int64_t>(max_value)) {
        std::ostringstream oss;
        oss << "value " << raw << " is out of range [" << +min_value << ", " << +max_value << "]";
        fail(where, oss.str());
    }
    return static_cast<T>(raw);
}

double get_number(const json& obj, const char* key, double fallback, const std::string& path) {
    if (!obj.contains(key)) {
        return fallback;
    }
    const json& v = obj.at(key);
    if (!v.is_number()) {
        fail(join_path(path, key), "expected a number");
    }
    const double d = v.get<double>();
    if (!std::isfinite(d)) {
        fail(join_path(path, key), "expected a finite number");
    }
    return d;
}

std::optional<double> get_optional_number(const json& obj, const char* key, const std::string& path) {
    if (!obj.contains(key) || obj.at(key).is_null()) {
        return std::nullopt;
    }
    return get_number(obj, key, 0.0, path);
}

std::string get_string(const json& obj,
                       const char* key,
                       const std::string& fallback,
                       const std::string& path) {
    if (!obj.contains(key)) {
        return fallback;
    }
    const json& v = obj.at(key);
    if (!v.is_string()) {
        fail(join_path(path, key), "expected a string");
    }
    return v.get<std::string>();
}

bool get_bool(const json& obj, const char* key, bool fallback, const std::string& path) {
    if (!obj.contains(key)) {
        return fallback;
    }
    const json& v = obj.at(key);
    if (!v.is_boolean()) {
        fail(join_path(path, key), "expected true or false");
    }
    return v.get<bool>();
}

template <typename Enum>
Enum get_enum(const json& obj,
              const char* key,
              Enum fallback,
              const std::string& path,
              std::initializer_list<std::pair<const char*, Enum>> options) {
    const std::string raw = get_string(obj, key, "", path);
    if (raw.empty()) {
        return fallback;
    }
    std::string expected;
    for (const auto& [name, value] : options) {
        if (raw == name) {
            return value;
        }
        expected += expected.empty() ? name : std::string(", ") + name;
    }
    fail(join_path(path, key), "unknown value '" + raw + "' (expected one of: " + expected + ")");
}

DetectionConfig default_detection() {
    DetectionConfig d;
    DetectorConfig zscore;
    zscore.type = "zscore";
    DetectorConfig ewma;
    ewma.type = "ewma";
    d.detectors = {zscore, ewma};
    d.votes_required = 1;
    return d;
}

DetectorConfig parse_detector(const json& j, const std::string& path) {
    if (!j.is_object()) {
        fail(path, "expected an object");
    }
    DetectorConfig d;
    d.type = get_string(j, "type", "", path);
    if (d.type == "zscore") {
        d.window = get_int<std::size_t>(j, "window", 60, path, 2, 100000);
        d.threshold = get_number(j, "threshold", 3.0, path);
    } else if (d.type == "ewma") {
        d.alpha = get_number(j, "alpha", 0.2, path);
        d.threshold = get_number(j, "threshold", 3.0, path);
        if (d.alpha <= 0.0 || d.alpha > 1.0) {
            fail(join_path(path, "alpha"), "must be in (0, 1]");
        }
    } else if (d.type == "cusum") {
        if (!j.contains("mean") || !j.contains("stddev")) {
            fail(path, "cusum needs the sensor's normal 'mean' and 'stddev'");
        }
        d.mean = get_number(j, "mean", 0.0, path);
        d.stddev = get_number(j, "stddev", 1.0, path);
        d.slack = get_number(j, "slack", 0.5, path);
        d.threshold = get_number(j, "threshold", 5.0, path);
        if (d.stddev <= 0.0) {
            fail(join_path(path, "stddev"), "must be greater than 0");
        }
        if (d.slack < 0.0) {
            fail(join_path(path, "slack"), "must not be negative");
        }
    } else {
        fail(join_path(path, "type"), "unknown detector '" + d.type + "' (expected zscore, ewma or cusum)");
    }
    if (d.threshold <= 0.0) {
        fail(join_path(path, "threshold"), "must be greater than 0");
    }
    return d;
}

DetectionConfig parse_detection(const json& j, const std::string& path) {
    DetectionConfig d = default_detection();
    if (!j.is_object()) {
        fail(path, "expected an object");
    }
    if (j.contains("detectors")) {
        const json& list = j.at("detectors");
        if (!list.is_array()) {
            fail(join_path(path, "detectors"), "expected an array");
        }
        d.detectors.clear();
        for (std::size_t i = 0; i < list.size(); ++i) {
            d.detectors.push_back(parse_detector(list[i], path + ".detectors[" + std::to_string(i) + "]"));
        }
    }
    d.cooldown_seconds = get_int<std::uint32_t>(j, "cooldown_seconds", d.cooldown_seconds, path, 0, 86400);
    const std::size_t max_votes = std::max<std::size_t>(1, d.detectors.size());
    d.votes_required = get_int<std::size_t>(j, "votes_required", 1, path, 1, max_votes);
    return d;
}

SensorConfig parse_sensor(const json& j, const std::string& path) {
    if (!j.is_object()) {
        fail(path, "expected an object");
    }
    SensorConfig s;
    s.id = get_string(j, "id", "", path);
    if (!is_valid_id(s.id)) {
        fail(join_path(path, "id"),
             "'" + s.id + "' is not a valid id (1-64 characters: letters, digits, '_', '-', '.')");
    }
    s.name = get_string(j, "name", s.id, path);
    s.unit = get_string(j, "unit", "", path);
    s.register_type =
        get_enum<RegisterType>(j,
                               "register_type",
                               RegisterType::holding,
                               path,
                               {{"holding", RegisterType::holding}, {"input", RegisterType::input}});
    s.data_type = get_enum<DataType>(j,
                                     "data_type",
                                     DataType::uint16,
                                     path,
                                     {{"uint16", DataType::uint16},
                                      {"int16", DataType::int16},
                                      {"uint32", DataType::uint32},
                                      {"int32", DataType::int32},
                                      {"float32", DataType::float32}});
    s.word_order = get_enum<WordOrder>(
        j, "word_order", WordOrder::big, path, {{"big", WordOrder::big}, {"little", WordOrder::little}});
    s.address = get_int<std::uint16_t>(j, "address", 0, path, 0, 65535);
    if (static_cast<std::uint32_t>(s.address) + register_count(s.data_type) > 65536U) {
        fail(join_path(path, "address"), "value does not fit below register 65535");
    }
    s.scale = get_number(j, "scale", 1.0, path);
    s.offset = get_number(j, "offset", 0.0, path);
    if (s.scale == 0.0) {
        fail(join_path(path, "scale"), "must not be 0");
    }

    if (j.contains("limits")) {
        const json& limits = j.at("limits");
        const std::string lpath = join_path(path, "limits");
        if (!limits.is_object()) {
            fail(lpath, "expected an object");
        }
        s.limits.low = get_optional_number(limits, "low", lpath);
        s.limits.high = get_optional_number(limits, "high", lpath);
        if (s.limits.low && s.limits.high && *s.limits.low >= *s.limits.high) {
            fail(lpath, "'low' must be less than 'high'");
        }
    }

    s.detection = j.contains("detection") ? parse_detection(j.at("detection"), join_path(path, "detection"))
                                          : default_detection();
    return s;
}

ModbusDeviceConfig parse_device(const json& d, const std::string& path) {
    if (!d.is_object()) {
        fail(path, "expected an object");
    }
    ModbusDeviceConfig device;
    device.id = get_string(d, "id", "", path);
    if (!is_valid_id(device.id)) {
        fail(join_path(path, "id"),
             "'" + device.id + "' is not a valid id (1-64 characters: letters, digits, '_', '-', '.')");
    }
    device.host = expand_env(get_string(d, "host", "", path));
    if (device.host.empty()) {
        fail(join_path(path, "host"), "is required");
    }
    device.port = get_int<std::uint16_t>(d, "port", 502, path, 1, 65535);
    device.unit_id = get_int<std::uint8_t>(d, "unit_id", 1, path, 0, 255);
    device.poll_interval_ms = get_int<std::uint32_t>(d, "poll_interval_ms", 1000, path, 50, 86'400'000);
    device.timeout_ms = get_int<std::uint32_t>(d, "timeout_ms", 3000, path, 100, 60'000);

    if (d.contains("sensors")) {
        const json& list = d.at("sensors");
        if (!list.is_array() || list.empty()) {
            fail(join_path(path, "sensors"), "expected a non-empty array");
        }
        for (std::size_t i = 0; i < list.size(); ++i) {
            device.sensors.push_back(parse_sensor(list[i], path + ".sensors[" + std::to_string(i) + "]"));
        }
    } else {
        // Backward-compatible shorthand from earlier versions: a device
        // without a sensor list is one uint16 value in holding register 0,
        // reported under the device's own id.
        SensorConfig sensor;
        sensor.id = device.id;
        sensor.name = device.id;
        sensor.detection = default_detection();
        device.sensors.push_back(std::move(sensor));
    }
    return device;
}

NotificationChannelConfig parse_channel(const json& j, const std::string& path) {
    if (!j.is_object()) {
        fail(path, "expected an object");
    }
    NotificationChannelConfig c;
    c.type = get_string(j, "type", "", path);
    if (c.type != "telegram" && c.type != "slack" && c.type != "webhook") {
        fail(join_path(path, "type"),
             "unknown channel '" + c.type + "' (expected telegram, slack or webhook)");
    }
    c.url = expand_env(get_string(j, "url", "", path));
    c.bot_token = expand_env(get_string(j, "bot_token", "", path));
    c.chat_id = expand_env(get_string(j, "chat_id", "", path));
    if (j.contains("headers")) {
        const json& headers = j.at("headers");
        if (!headers.is_object()) {
            fail(join_path(path, "headers"), "expected an object");
        }
        for (const auto& [name, value] : headers.items()) {
            if (!value.is_string()) {
                fail(join_path(path, "headers." + name), "expected a string");
            }
            c.headers[name] = expand_env(value.get<std::string>());
        }
    }
    return c;
}

bool channel_is_complete(const NotificationChannelConfig& c) {
    if (c.type == "telegram") {
        return !c.bot_token.empty() && !c.chat_id.empty();
    }
    return !c.url.empty();
}

/// Applies IRONPULSE_* environment variable overrides on top of whatever
/// was loaded from the JSON file. This is what lets a single Docker
/// Compose `.env` value (e.g. HTTP_PORT) simultaneously control the port
/// the container's host side maps *and* the port ironpulse binds to
/// inside the container — without editing two files that have to stay in
/// sync by hand. Standard twelve-factor-app practice: config that varies
/// between environments belongs in the environment, not baked into a
/// checked-in file.
void apply_env_overrides(AppConfig& cfg) {
    if (auto v = env("IRONPULSE_HTTP_PORT"))
        cfg.http_port = parse_or<std::uint16_t>(v, cfg.http_port);
    if (auto v = env("IRONPULSE_WS_PORT"))
        cfg.ws_port = parse_or<std::uint16_t>(v, cfg.ws_port);
    if (auto v = env("IRONPULSE_LOG_LEVEL"))
        cfg.log_level = *v;
    if (auto v = env("IRONPULSE_LOG_FILE"))
        cfg.log_file = *v;
    if (auto v = env("IRONPULSE_WEB_ROOT"))
        cfg.web_root = *v;
    if (auto v = env("IRONPULSE_DATA_DIR"))
        cfg.data_dir = *v;
    if (auto v = env("IRONPULSE_WORKER_THREADS"))
        cfg.worker_threads = parse_or<std::size_t>(v, cfg.worker_threads);
    if (auto v = env("IRONPULSE_PERSISTENCE_ENABLED"))
        cfg.persistence_enabled = parse_or<bool>(v, cfg.persistence_enabled);
    if (auto v = env("IRONPULSE_RETENTION_HOURS"))
        cfg.retention_hours = parse_or<std::uint32_t>(v, cfg.retention_hours);
    if (auto v = env("IRONPULSE_API_TOKEN"))
        cfg.api_token = *v;

    // Notification shortcuts, so a Docker deployment can enable a channel
    // from .env alone without touching the JSON config.
    if (auto v = env("IRONPULSE_NOTIFY_LANGUAGE"))
        cfg.notifications.language = *v;
    if (auto v = env("IRONPULSE_PUBLIC_URL"))
        cfg.notifications.dashboard_url = *v;
    if (auto token = env("IRONPULSE_TELEGRAM_BOT_TOKEN")) {
        NotificationChannelConfig c;
        c.type = "telegram";
        c.bot_token = *token;
        c.chat_id = env("IRONPULSE_TELEGRAM_CHAT_ID").value_or("");
        cfg.notifications.channels.push_back(std::move(c));
    }
    if (auto url = env("IRONPULSE_SLACK_WEBHOOK_URL")) {
        NotificationChannelConfig c;
        c.type = "slack";
        c.url = *url;
        cfg.notifications.channels.push_back(std::move(c));
    }
    if (auto url = env("IRONPULSE_WEBHOOK_URL")) {
        NotificationChannelConfig c;
        c.type = "webhook";
        c.url = *url;
        cfg.notifications.channels.push_back(std::move(c));
    }
}

void validate(AppConfig& cfg) {
    static const std::set<std::string> kLogLevels{"trace", "debug", "info", "warn", "error", "critical"};
    if (!kLogLevels.contains(cfg.log_level)) {
        fail("log_level",
             "unknown value '" + cfg.log_level + "' (expected trace, debug, info, warn, error or critical)");
    }
    if (cfg.http_port == 0 || cfg.ws_port == 0) {
        fail("http_port/ws_port", "ports must be between 1 and 65535");
    }
    if (cfg.http_port == cfg.ws_port) {
        fail("ws_port", "must differ from http_port");
    }
    if (cfg.worker_threads == 0 || cfg.worker_threads > 256) {
        fail("worker_threads", "must be between 1 and 256");
    }
    if (cfg.retention_hours == 0) {
        fail("retention_hours", "must be at least 1");
    }

    std::set<std::string> device_ids;
    std::set<std::string> sensor_ids;
    for (std::size_t i = 0; i < cfg.devices.size(); ++i) {
        const auto& device = cfg.devices[i];
        if (!device_ids.insert(device.id).second) {
            fail("devices[" + std::to_string(i) + "].id", "duplicate device id '" + device.id + "'");
        }
        for (const auto& sensor : device.sensors) {
            if (!sensor_ids.insert(sensor.id).second) {
                fail("devices[" + std::to_string(i) + "]", "duplicate sensor id '" + sensor.id + "'");
            }
        }
    }

    if (!is_supported_language(cfg.notifications.language)) {
        fail("notifications.language",
             "unsupported language '" + cfg.notifications.language +
                 "' (expected ru, en, zh, hi, es, fr, de or it)");
    }

    // A channel whose credentials come from an unset environment variable
    // is dropped with a warning rather than failing startup: monitoring
    // without notifications is better than no monitoring at all.
    auto& channels = cfg.notifications.channels;
    for (auto it = channels.begin(); it != channels.end();) {
        if (!channel_is_complete(*it)) {
            cfg.warnings.push_back("notification channel '" + it->type +
                                   "' is missing its URL or credentials and was disabled");
            it = channels.erase(it);
        } else {
            ++it;
        }
    }
}

AppConfig parse(const json& j) {
    if (!j.is_object()) {
        fail("(root)", "expected a JSON object");
    }

    AppConfig cfg;
    cfg.app_name = get_string(j, "app_name", cfg.app_name, "");
    cfg.log_level = get_string(j, "log_level", cfg.log_level, "");
    cfg.log_file = get_string(j, "log_file", cfg.log_file, "");
    cfg.http_port = get_int<std::uint16_t>(j, "http_port", cfg.http_port, "", 1, 65535);
    cfg.ws_port = get_int<std::uint16_t>(j, "ws_port", cfg.ws_port, "", 1, 65535);
    cfg.worker_threads = get_int<std::size_t>(j, "worker_threads", cfg.worker_threads, "", 1, 256);
    cfg.web_root = get_string(j, "web_root", cfg.web_root, "");
    cfg.data_dir = get_string(j, "data_dir", cfg.data_dir, "");
    cfg.persistence_enabled = get_bool(j, "persistence_enabled", cfg.persistence_enabled, "");
    cfg.retention_hours = get_int<std::uint32_t>(j, "retention_hours", cfg.retention_hours, "", 1, 87600);
    cfg.api_token = expand_env(get_string(j, "api_token", "", ""));

    if (j.contains("devices")) {
        const json& list = j.at("devices");
        if (!list.is_array()) {
            fail("devices", "expected an array");
        }
        for (std::size_t i = 0; i < list.size(); ++i) {
            cfg.devices.push_back(parse_device(list[i], "devices[" + std::to_string(i) + "]"));
        }
    }

    if (j.contains("notifications")) {
        const json& n = j.at("notifications");
        if (!n.is_object()) {
            fail("notifications", "expected an object");
        }
        cfg.notifications.language = get_string(n, "language", cfg.notifications.language, "notifications");
        cfg.notifications.dashboard_url = expand_env(get_string(n, "dashboard_url", "", "notifications"));
        cfg.notifications.min_severity =
            get_enum<Severity>(n,
                               "min_severity",
                               Severity::warning,
                               "notifications",
                               {{"warning", Severity::warning}, {"critical", Severity::critical}});
        if (n.contains("channels")) {
            const json& list = n.at("channels");
            if (!list.is_array()) {
                fail("notifications.channels", "expected an array");
            }
            for (std::size_t i = 0; i < list.size(); ++i) {
                cfg.notifications.channels.push_back(
                    parse_channel(list[i], "notifications.channels[" + std::to_string(i) + "]"));
            }
        }
    }

    apply_env_overrides(cfg);
    validate(cfg);
    return cfg;
}

}  // namespace

std::string expand_env(const std::string& text) {
    std::string result;
    result.reserve(text.size());
    std::size_t pos = 0;
    while (pos < text.size()) {
        const std::size_t start = text.find("${", pos);
        if (start == std::string::npos) {
            result.append(text, pos, std::string::npos);
            break;
        }
        const std::size_t end = text.find('}', start + 2);
        if (end == std::string::npos) {
            result.append(text, pos, std::string::npos);
            break;
        }
        result.append(text, pos, start - pos);
        const std::string name = text.substr(start + 2, end - start - 2);
        if (const char* value = std::getenv(name.c_str())) {
            result += value;
        }
        pos = end + 1;
    }
    return result;
}

bool is_valid_id(const std::string& id) {
    if (id.empty() || id.size() > 64 || id.front() == '.') {
        return false;
    }
    return std::all_of(id.begin(), id.end(), [](char c) {
        return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' ||
               c == '-' || c == '.';
    });
}

bool is_supported_language(const std::string& code) {
    static const std::set<std::string> kLanguages{"ru", "en", "zh", "hi", "es", "fr", "de", "it"};
    return kLanguages.contains(code);
}

AppConfig AppConfig::load_from_string(const std::string& json_text) {
    json j;
    try {
        j = json::parse(json_text);
    } catch (const json::parse_error& e) {
        throw std::runtime_error(std::string("config: invalid JSON: ") + e.what());
    }
    return parse(j);
}

AppConfig AppConfig::load_from_file(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open config file: " + path);
    }
    std::stringstream buffer;
    buffer << file.rdbuf();
    return load_from_string(buffer.str());
}

}  // namespace ironpulse::core
