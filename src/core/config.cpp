#include "ironpulse/core/config.hpp"

#include <cstdlib>
#include <fstream>
#include <nlohmann/json.hpp>
#include <optional>
#include <stdexcept>
#include <string>
#include <type_traits>

namespace ironpulse::core {

namespace {

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
            return static_cast<T>(std::stoul(*raw));
        }
    } catch (const std::exception&) {
        return fallback;
    }
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
}

}  // namespace

AppConfig AppConfig::load_from_file(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open config file: " + path);
    }

    nlohmann::json j;
    file >> j;

    AppConfig cfg;
    cfg.app_name = j.value("app_name", cfg.app_name);
    cfg.log_level = j.value("log_level", cfg.log_level);
    cfg.log_file = j.value("log_file", cfg.log_file);
    cfg.http_port = j.value("http_port", cfg.http_port);
    cfg.ws_port = j.value("ws_port", cfg.ws_port);
    cfg.worker_threads = j.value("worker_threads", cfg.worker_threads);
    cfg.web_root = j.value("web_root", cfg.web_root);
    cfg.data_dir = j.value("data_dir", cfg.data_dir);
    cfg.persistence_enabled = j.value("persistence_enabled", cfg.persistence_enabled);
    cfg.retention_hours = j.value("retention_hours", cfg.retention_hours);

    if (j.contains("devices")) {
        for (const auto& d : j.at("devices")) {
            ModbusDeviceConfig device;
            device.id = d.at("id").get<std::string>();
            device.host = d.at("host").get<std::string>();
            device.port = d.value("port", static_cast<std::uint16_t>(502));
            device.unit_id = d.value("unit_id", static_cast<std::uint8_t>(1));
            device.poll_interval_ms = d.value("poll_interval_ms", static_cast<std::uint32_t>(1000));
            cfg.devices.push_back(std::move(device));
        }
    }

    apply_env_overrides(cfg);
    return cfg;
}

}  // namespace ironpulse::core
