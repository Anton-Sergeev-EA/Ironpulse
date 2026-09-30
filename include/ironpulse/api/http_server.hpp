#pragma once

#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include "ironpulse/core/alert_log.hpp"
#include "ironpulse/core/config.hpp"
#include "ironpulse/core/device_registry.hpp"
#include "ironpulse/core/metrics.hpp"
#include "ironpulse/storage/series_store.hpp"

// Forward-declared to keep httplib.h (a sizeable single header) out of
// this public header; only http_server.cpp needs the full definition.
namespace httplib {
class Server;
struct Request;
}  // namespace httplib

namespace ironpulse::api {

struct HttpServerOptions {
    std::string bind_address = "0.0.0.0";
    std::uint16_t port = 8080;
    std::uint16_t ws_port = 8081;
    std::filesystem::path web_root = "web";
    /// When non-empty, /api/v1/* (except /api/v1/config) and /metrics
    /// require "Authorization: Bearer <token>" or "?token=<token>".
    std::string api_token;
};

/// REST API server, backed by cpp-httplib, running on its own thread.
/// The full contract is documented in docs/openapi.yaml.
///
///   GET /api/v1/devices             configured devices with status and sensor ids
///   GET /api/v1/sensors             sensor metadata, limits and latest value
///   GET /api/v1/series/{sensor_id}  history (?since=<seconds>, ?format=csv)
///   GET /api/v1/alerts              recent alerts, newest first (?limit=<n>)
///   GET /api/v1/config              what the dashboard needs to bootstrap
///   GET /metrics                    Prometheus exposition format
///   GET /healthz                    liveness probe (never authenticated)
///   GET /*                          the dashboard (static files from web_root)
class HttpServer {
public:
    HttpServer(storage::SeriesStore& store,
               core::AlertLog& alerts,
               core::DeviceRegistry& devices,
               const std::vector<core::ModbusDeviceConfig>& device_configs,
               core::Metrics& metrics,
               HttpServerOptions options);
    ~HttpServer();

    HttpServer(const HttpServer&) = delete;
    HttpServer& operator=(const HttpServer&) = delete;

    /// Starts the server on a background thread. Returns true once the
    /// port is bound, false if binding failed.
    bool start();
    void stop();

    /// The port actually bound (differs from the option when it was 0).
    [[nodiscard]] std::uint16_t port() const noexcept {
        return bound_port_;
    }

private:
    void setup_routes();
    [[nodiscard]] bool authorized(const httplib::Request& req) const;
    [[nodiscard]] const core::SensorConfig* find_sensor(const std::string& sensor_id) const;

    storage::SeriesStore& store_;
    core::AlertLog& alerts_;
    core::DeviceRegistry& devices_;
    const std::vector<core::ModbusDeviceConfig>& device_configs_;
    core::Metrics& metrics_;
    HttpServerOptions options_;
    std::uint16_t bound_port_ = 0;

    std::unique_ptr<httplib::Server> server_;
    std::thread server_thread_;
};

/// Compares two secrets in time independent of where they first differ,
/// so response timing does not leak how much of a guessed token matched.
[[nodiscard]] bool constant_time_equals(const std::string& a, const std::string& b) noexcept;

/// Formats a time point as ISO 8601 UTC with milliseconds.
[[nodiscard]] std::string to_iso8601(std::chrono::system_clock::time_point tp);

}  // namespace ironpulse::api
