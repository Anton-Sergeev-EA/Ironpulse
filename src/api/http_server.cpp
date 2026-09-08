#include "ironpulse/api/http_server.hpp"

#include <httplib.h>

#include <chrono>
#include <ctime>
#include <iomanip>
#include <nlohmann/json.hpp>
#include <sstream>

#include "ironpulse/core/logger.hpp"

namespace ironpulse::api {

namespace {

std::string to_iso8601(std::chrono::system_clock::time_point tp) {
    const std::time_t time = std::chrono::system_clock::to_time_t(tp);
    const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(tp.time_since_epoch()) % 1000;

    std::tm utc_tm{};
#if defined(_WIN32)
    gmtime_s(&utc_tm, &time);
#else
    gmtime_r(&time, &utc_tm);
#endif

    std::ostringstream oss;
    oss << std::put_time(&utc_tm, "%Y-%m-%dT%H:%M:%S");
    oss << '.' << std::setfill('0') << std::setw(3) << ms.count() << 'Z';
    return oss.str();
}

}  // namespace

HttpServer::HttpServer(storage::SeriesStore& store,
                       core::AlertLog& alerts,
                       core::DeviceRegistry& devices,
                       std::uint16_t port,
                       std::uint16_t ws_port,
                       std::filesystem::path web_root)
    : store_(store),
      alerts_(alerts),
      devices_(devices),
      port_(port),
      ws_port_(ws_port),
      web_root_(std::move(web_root)),
      server_(std::make_unique<httplib::Server>()) {
    setup_routes();
}

HttpServer::~HttpServer() {
    stop();
}

void HttpServer::setup_routes() {
    if (std::filesystem::exists(web_root_)) {
        server_->set_mount_point("/", web_root_.string());
    } else {
        IP_LOG_WARN("Web root '{}' does not exist — dashboard static files will not be served",
                    web_root_.string());
    }

    server_->set_default_headers({{"Access-Control-Allow-Origin", "*"}});

    server_->Get("/api/v1/devices", [this](const httplib::Request&, httplib::Response& res) {
        nlohmann::json body = nlohmann::json::array();
        for (const auto& device : devices_.all()) {
            body.push_back({{"id", device.device_id}, {"online", device.online}});
        }
        res.set_content(body.dump(), "application/json");
    });

    server_->Get(R"(/api/v1/series/([^/]+))", [this](const httplib::Request& req, httplib::Response& res) {
        const std::string sensor_id = req.matches[1];
        const int since_seconds = req.has_param("since") ? std::stoi(req.get_param_value("since")) : 300;
        const auto cutoff = std::chrono::system_clock::now() - std::chrono::seconds(since_seconds);

        // recent() returns the buffer's full retained window; filter to
        // the requested time range (the ring buffer capacity, not this
        // query parameter, bounds how far back data can possibly exist).
        auto samples = store_.recent(sensor_id, 100000);

        nlohmann::json body = nlohmann::json::array();
        for (const auto& sample : samples) {
            if (sample.timestamp < cutoff) {
                continue;
            }
            body.push_back({{"timestamp", to_iso8601(sample.timestamp)}, {"value", sample.value}});
        }
        res.set_content(body.dump(), "application/json");
    });

    server_->Get("/api/v1/alerts", [this](const httplib::Request& req, httplib::Response& res) {
        const std::size_t limit =
            req.has_param("limit") ? static_cast<std::size_t>(std::stoul(req.get_param_value("limit"))) : 50;

        nlohmann::json body = nlohmann::json::array();
        for (const auto& event : alerts_.recent(limit)) {
            body.push_back({
                {"sensor_id", event.sensor_id},
                {"detector", event.detector_name},
                {"message", event.message},
                {"score", event.score},
                {"confidence", event.confidence},
                {"timestamp", to_iso8601(event.timestamp)},
            });
        }
        res.set_content(body.dump(), "application/json");
    });

    server_->Get("/healthz", [](const httplib::Request&, httplib::Response& res) {
        res.set_content(R"({"status":"ok"})", "application/json");
    });

    server_->Get("/api/v1/config", [this](const httplib::Request& req, httplib::Response& res) {
        // Lets the dashboard discover the WebSocket port at runtime instead
        // of hardcoding it — the REST and WS servers listen on different
        // ports (see main.cpp), and that mapping can vary between a bare
        // local run and a docker-compose/nginx deployment.
        nlohmann::json body{
            {"ws_port", ws_port_},
            {"ws_host", req.get_header_value("Host").substr(0, req.get_header_value("Host").find(':'))},
        };
        res.set_content(body.dump(), "application/json");
    });
}

void HttpServer::start() {
    server_thread_ = std::thread([this] {
        IP_LOG_INFO("REST API listening on port {}", port_);
        if (!server_->listen("0.0.0.0", port_)) {
            IP_LOG_ERROR("Failed to bind REST API to port {}", port_);
        }
    });

    // Give the listener a moment to bind before returning, so callers
    // logging "server started" reflect reality.
    for (int i = 0; i < 50 && !server_->is_running(); ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

void HttpServer::stop() {
    if (server_ && server_->is_running()) {
        server_->stop();
    }
    if (server_thread_.joinable()) {
        server_thread_.join();
    }
}

}  // namespace ironpulse::api
