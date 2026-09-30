#include "ironpulse/api/http_server.hpp"

#include <httplib.h>

#include <chrono>
#include <ctime>
#include <iomanip>
#include <nlohmann/json.hpp>
#include <optional>
#include <sstream>

#include "ironpulse/api/serialization.hpp"
#include "ironpulse/core/logger.hpp"
#include "ironpulse/version.hpp"

namespace ironpulse::api {

namespace {

constexpr std::size_t kMaxJsonPoints = 10000;

void send_json(httplib::Response& res, const nlohmann::json& body, int status = 200) {
    res.status = status;
    res.set_content(body.dump(), "application/json");
}

void send_error(httplib::Response& res, int status, const std::string& message) {
    send_json(res, nlohmann::json{{"error", message}}, status);
}

/// Parses an optional integer query parameter within [min, max]. Returns
/// std::nullopt (after writing a 400 response) if it is malformed or out
/// of range — the previous std::stoi call turned "?limit=abc" into a 500.
std::optional<long long> int_param(const httplib::Request& req,
                                   httplib::Response& res,
                                   const char* name,
                                   long long fallback,
                                   long long min_value,
                                   long long max_value) {
    if (!req.has_param(name)) {
        return fallback;
    }
    const std::string raw = req.get_param_value(name);
    try {
        std::size_t consumed = 0;
        const long long value = std::stoll(raw, &consumed);
        if (consumed == raw.size() && value >= min_value && value <= max_value) {
            return value;
        }
    } catch (const std::exception&) {
    }
    send_error(res,
               400,
               std::string("parameter '") + name + "' must be an integer between " +
                   std::to_string(min_value) + " and " + std::to_string(max_value));
    return std::nullopt;
}

std::string host_without_port(const std::string& host) {
    if (!host.empty() && host.front() == '[') {  // IPv6 literal, e.g. [::1]:8080
        const auto end = host.find(']');
        return end == std::string::npos ? host : host.substr(0, end + 1);
    }
    return host.substr(0, host.find(':'));
}

nlohmann::json limits_json(const core::Limits& limits) {
    nlohmann::json j = nlohmann::json::object();
    j["low"] = limits.low ? nlohmann::json(*limits.low) : nlohmann::json(nullptr);
    j["high"] = limits.high ? nlohmann::json(*limits.high) : nlohmann::json(nullptr);
    return j;
}

}  // namespace

bool constant_time_equals(const std::string& a, const std::string& b) noexcept {
    unsigned char diff = a.size() == b.size() ? 0 : 1;
    const std::size_t n = std::max(a.size(), b.size());
    for (std::size_t i = 0; i < n; ++i) {
        const auto ca = static_cast<unsigned char>(i < a.size() ? a[i] : 0);
        const auto cb = static_cast<unsigned char>(i < b.size() ? b[i] : 0);
        diff = static_cast<unsigned char>(diff | (ca ^ cb));
    }
    return diff == 0;
}

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

HttpServer::HttpServer(storage::SeriesStore& store,
                       core::AlertLog& alerts,
                       core::DeviceRegistry& devices,
                       const std::vector<core::ModbusDeviceConfig>& device_configs,
                       core::Metrics& metrics,
                       HttpServerOptions options)
    : store_(store),
      alerts_(alerts),
      devices_(devices),
      device_configs_(device_configs),
      metrics_(metrics),
      options_(std::move(options)),
      server_(std::make_unique<httplib::Server>()) {
    setup_routes();
}

HttpServer::~HttpServer() {
    stop();
}

bool HttpServer::authorized(const httplib::Request& req) const {
    if (options_.api_token.empty()) {
        return true;
    }
    const std::string header = req.get_header_value("Authorization");
    constexpr std::string_view kBearer = "Bearer ";
    if (header.size() > kBearer.size() && header.compare(0, kBearer.size(), kBearer) == 0) {
        return constant_time_equals(header.substr(kBearer.size()), options_.api_token);
    }
    // Query-string fallback for clients that cannot set headers (a plain
    // download link for CSV export, some monitoring agents).
    if (req.has_param("token")) {
        return constant_time_equals(req.get_param_value("token"), options_.api_token);
    }
    return false;
}

const core::SensorConfig* HttpServer::find_sensor(const std::string& sensor_id) const {
    for (const auto& device : device_configs_) {
        for (const auto& sensor : device.sensors) {
            if (sensor.id == sensor_id) {
                return &sensor;
            }
        }
    }
    return nullptr;
}

void HttpServer::setup_routes() {
    if (std::filesystem::exists(options_.web_root)) {
        server_->set_mount_point("/", options_.web_root.string());
    } else {
        IP_LOG_WARN("Web root '{}' does not exist — dashboard static files will not be served",
                    options_.web_root.string());
    }

    server_->set_default_headers({
        {"Access-Control-Allow-Origin", "*"},
        {"Access-Control-Allow-Headers", "Authorization"},
        {"X-Content-Type-Options", "nosniff"},
        {"Referrer-Policy", "no-referrer"},
    });

    // Authentication gate, evaluated before any route. /api/v1/config stays
    // open so the dashboard can learn that it has to ask for a token.
    server_->set_pre_routing_handler([this](const httplib::Request& req, httplib::Response& res) {
        const bool is_api = req.path.rfind("/api/", 0) == 0 && req.path != "/api/v1/config";
        const bool protected_path = is_api || req.path == "/metrics";
        if (req.method == "OPTIONS" || !protected_path || authorized(req)) {
            return httplib::Server::HandlerResponse::Unhandled;
        }
        res.set_header("WWW-Authenticate", "Bearer");
        send_error(res, 401, "unauthorized");
        return httplib::Server::HandlerResponse::Handled;
    });

    server_->set_exception_handler(
        [](const httplib::Request& req, httplib::Response& res, std::exception_ptr ep) {
            std::string what = "unknown error";
            try {
                std::rethrow_exception(ep);
            } catch (const std::exception& e) {
                what = e.what();
            } catch (...) {
            }
            IP_LOG_ERROR("HTTP {} {} failed: {}", req.method, req.path, what);
            send_error(res, 500, "internal server error");
        });

    server_->Options(R"(/.*)", [](const httplib::Request&, httplib::Response& res) { res.status = 204; });

    server_->Get("/api/v1/devices", [this](const httplib::Request&, httplib::Response& res) {
        nlohmann::json body = nlohmann::json::array();
        for (const auto& device : device_configs_) {
            nlohmann::json sensors = nlohmann::json::array();
            for (const auto& sensor : device.sensors) {
                sensors.push_back(sensor.id);
            }
            const auto status = devices_.status(device.id);
            body.push_back({
                {"id", device.id},
                {"online", status.value_or(false)},
                {"poll_interval_ms", device.poll_interval_ms},
                {"sensors", sensors},
            });
        }
        send_json(res, body);
    });

    server_->Get("/api/v1/sensors", [this](const httplib::Request&, httplib::Response& res) {
        nlohmann::json body = nlohmann::json::array();
        for (const auto& device : device_configs_) {
            for (const auto& sensor : device.sensors) {
                nlohmann::json latest = nullptr;
                if (auto sample = store_.latest(sensor.id)) {
                    latest = {{"timestamp", to_iso8601(sample->timestamp)}, {"value", sample->value}};
                }
                body.push_back({
                    {"id", sensor.id},
                    {"name", sensor.name},
                    {"unit", sensor.unit},
                    {"device_id", device.id},
                    {"limits", limits_json(sensor.limits)},
                    {"latest", latest},
                });
            }
        }
        send_json(res, body);
    });

    server_->Get(
        R"(/api/v1/series/([A-Za-z0-9_.\-]+))", [this](const httplib::Request& req, httplib::Response& res) {
            const std::string sensor_id = req.matches[1];
            if (find_sensor(sensor_id) == nullptr) {
                send_error(res, 404, "unknown sensor '" + sensor_id + "'");
                return;
            }
            // Up to ten years: the actual bound is what retention kept.
            const auto since = int_param(req, res, "since", 300, 1, 315'360'000);
            if (!since) {
                return;
            }
            const std::string format = req.has_param("format") ? req.get_param_value("format") : "json";
            if (format != "json" && format != "csv") {
                send_error(res, 400, "parameter 'format' must be 'json' or 'csv'");
                return;
            }

            const auto cutoff = std::chrono::system_clock::now() - std::chrono::seconds(*since);
            const auto samples = store_.history(sensor_id, cutoff);

            if (format == "csv") {
                std::string csv = "timestamp,value\n";
                csv.reserve(samples.size() * 40);
                std::ostringstream line;
                line.precision(15);
                for (const auto& sample : samples) {
                    line.str({});
                    line << to_iso8601(sample.timestamp) << ',' << sample.value << '\n';
                    csv += line.str();
                }
                res.set_header("Content-Disposition", "attachment; filename=\"" + sensor_id + ".csv\"");
                res.set_content(csv, "text/csv; charset=utf-8");
                return;
            }

            // Long ranges are thinned evenly so a chart request for a week of
            // 1 Hz data stays a few hundred KB; CSV export is never thinned.
            const std::size_t stride =
                samples.size() > kMaxJsonPoints ? (samples.size() + kMaxJsonPoints - 1) / kMaxJsonPoints : 1;
            nlohmann::json body = nlohmann::json::array();
            for (std::size_t i = 0; i < samples.size(); i += stride) {
                body.push_back(
                    {{"timestamp", to_iso8601(samples[i].timestamp)}, {"value", samples[i].value}});
            }
            send_json(res, body);
        });

    server_->Get("/api/v1/alerts", [this](const httplib::Request& req, httplib::Response& res) {
        const auto limit = int_param(req, res, "limit", 50, 1, 1000);
        if (!limit) {
            return;
        }
        nlohmann::json body = nlohmann::json::array();
        for (const auto& event : alerts_.recent(static_cast<std::size_t>(*limit))) {
            nlohmann::json j = alert_fields(event);
            j["timestamp"] = to_iso8601(event.timestamp);
            body.push_back(std::move(j));
        }
        send_json(res, body);
    });

    server_->Get("/api/v1/config", [this](const httplib::Request& req, httplib::Response& res) {
        // Lets the dashboard discover the WebSocket port at runtime instead
        // of hardcoding it — the REST and WS servers listen on different
        // ports, and that mapping can vary between a bare local run and a
        // docker-compose/nginx deployment.
        send_json(res,
                  {
                      {"ws_port", options_.ws_port},
                      {"ws_host", host_without_port(req.get_header_value("Host"))},
                      {"auth_required", !options_.api_token.empty()},
                      {"version", kVersion},
                  });
    });

    server_->Get("/metrics", [this](const httplib::Request&, httplib::Response& res) {
        res.set_content(metrics_.render(), "text/plain; version=0.0.4; charset=utf-8");
    });

    server_->Get("/healthz", [](const httplib::Request&, httplib::Response& res) {
        send_json(res, {{"status", "ok"}, {"version", kVersion}});
    });
}

bool HttpServer::start() {
    if (options_.port == 0) {  // ephemeral port (tests)
        const int port = server_->bind_to_any_port(options_.bind_address);
        if (port <= 0) {
            IP_LOG_ERROR("Failed to bind REST API to an ephemeral port");
            return false;
        }
        bound_port_ = static_cast<std::uint16_t>(port);
    } else {
        if (!server_->bind_to_port(options_.bind_address, options_.port)) {
            IP_LOG_ERROR("Failed to bind REST API to {}:{}", options_.bind_address, options_.port);
            return false;
        }
        bound_port_ = options_.port;
    }
    server_thread_ = std::thread([this] { server_->listen_after_bind(); });
    IP_LOG_INFO("REST API and dashboard listening on port {}", bound_port_);
    return true;
}

void HttpServer::stop() {
    if (server_) {
        server_->stop();
    }
    if (server_thread_.joinable()) {
        server_thread_.join();
    }
}

}  // namespace ironpulse::api
