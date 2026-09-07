#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace ironpulse::core {

struct ModbusDeviceConfig {
    std::string id;
    std::string host;
    std::uint16_t port = 502;
    std::uint8_t unit_id = 1;
    std::uint32_t poll_interval_ms = 1000;
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
    std::vector<ModbusDeviceConfig> devices;

    /// Loads configuration from a JSON file. Throws std::runtime_error on
    /// missing file or malformed content.
    static AppConfig load_from_file(const std::string& path);
};

}  // namespace ironpulse::core
