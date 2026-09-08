#pragma once

#include <cstdint>
#include <filesystem>
#include <memory>
#include <thread>

#include "ironpulse/core/alert_log.hpp"
#include "ironpulse/core/device_registry.hpp"
#include "ironpulse/storage/series_store.hpp"

// Forward-declared to keep httplib.h (a sizeable single header) out of
// this public header; only http_server.cpp needs the full definition.
namespace httplib {
class Server;
}

namespace ironpulse::api {

/// REST API server, backed by cpp-httplib, running on its own thread.
///
/// Endpoints:
///   GET /api/v1/devices             -> [{ "id": ..., "online": bool }, ...]
///   GET /api/v1/series/{sensor_id}  -> [{ "timestamp": ISO8601, "value": number }, ...]
///                                      (?since=<seconds>, default 300)
///   GET /api/v1/alerts              -> recent AnomalyEvents, newest first
///                                      (?limit=<n>, default 50)
///   GET /api/v1/config              -> { "ws_port": <n> }, so the dashboard
///                                      can find the WebSocket server without
///                                      hardcoding a port (see web/js/ws-client.js)
///   GET /*                          -> static files from the configured web root
///                                      (the dashboard in web/)
class HttpServer {
public:
    HttpServer(storage::SeriesStore& store,
               core::AlertLog& alerts,
               core::DeviceRegistry& devices,
               std::uint16_t port,
               std::uint16_t ws_port,
               std::filesystem::path web_root);
    ~HttpServer();

    HttpServer(const HttpServer&) = delete;
    HttpServer& operator=(const HttpServer&) = delete;

    /// Starts the server on a background thread. Returns once the server
    /// has bound its port (or immediately logs and returns on failure).
    void start();
    void stop();

private:
    void setup_routes();

    storage::SeriesStore& store_;
    core::AlertLog& alerts_;
    core::DeviceRegistry& devices_;
    std::uint16_t port_;
    std::uint16_t ws_port_;
    std::filesystem::path web_root_;

    std::unique_ptr<httplib::Server> server_;
    std::thread server_thread_;
};

}  // namespace ironpulse::api
