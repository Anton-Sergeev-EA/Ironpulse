#include <httplib.h>

#include <asio.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>
#include <chrono>
#include <nlohmann/json.hpp>
#include <thread>

#include "ironpulse/api/http_server.hpp"
#include "ironpulse/api/ws_server.hpp"

using Catch::Matchers::ContainsSubstring;
using Catch::Matchers::StartsWith;
using ironpulse::api::HttpServer;
using ironpulse::api::HttpServerOptions;

namespace {

/// A running HttpServer over a small in-memory model, on an ephemeral port.
struct ApiFixture {
    ironpulse::storage::SeriesStore store{64};
    ironpulse::core::AlertLog alerts{10};
    ironpulse::core::DeviceRegistry registry;
    ironpulse::core::Metrics metrics;
    std::vector<ironpulse::core::ModbusDeviceConfig> devices;
    std::unique_ptr<HttpServer> server;
    std::unique_ptr<httplib::Client> client;

    explicit ApiFixture(const std::string& token = {}) {
        ironpulse::core::SensorConfig temp;
        temp.id = "temp";
        temp.name = "Temperature";
        temp.unit = "°C";
        temp.limits.high = 90.0;
        ironpulse::core::ModbusDeviceConfig device;
        device.id = "plc";
        device.host = "127.0.0.1";
        device.sensors = {temp};
        devices = {device};

        const auto now = std::chrono::system_clock::now();
        store.record("temp", 20.5, now - std::chrono::seconds(30));
        store.record("temp", 21.5, now - std::chrono::seconds(10));
        registry.set_status("plc", true);
        metrics.set("ironpulse_device_up", {{"device", "plc"}}, 1);

        ironpulse::core::AnomalyEvent alert;
        alert.sensor_id = "temp";
        alert.kind = ironpulse::core::AlertKind::limit;
        alert.severity = ironpulse::core::Severity::critical;
        alert.value = 95.0;
        alert.limit = 90.0;
        alert.above_limit = true;
        alert.timestamp = now;
        alerts.push(alert);

        HttpServerOptions options;
        options.bind_address = "127.0.0.1";
        options.port = 0;
        options.web_root = "/nonexistent";
        options.api_token = token;
        server = std::make_unique<HttpServer>(store, alerts, registry, devices, metrics, options);
        REQUIRE(server->start());
        client = std::make_unique<httplib::Client>("127.0.0.1", server->port());
        client->set_read_timeout(std::chrono::seconds(5));
    }

    ~ApiFixture() {
        server->stop();
    }
};

}  // namespace

TEST_CASE("API lists devices and sensors with metadata and latest values", "[api]") {
    ApiFixture api;

    auto devices = api.client->Get("/api/v1/devices");
    REQUIRE(devices);
    REQUIRE(devices->status == 200);
    const auto d = nlohmann::json::parse(devices->body);
    CHECK(d[0]["id"] == "plc");
    CHECK(d[0]["online"] == true);
    CHECK(d[0]["sensors"][0] == "temp");

    auto sensors = api.client->Get("/api/v1/sensors");
    REQUIRE(sensors);
    const auto s = nlohmann::json::parse(sensors->body);
    CHECK(s[0]["name"] == "Temperature");
    CHECK(s[0]["unit"] == "°C");
    CHECK(s[0]["limits"]["high"] == 90.0);
    CHECK(s[0]["limits"]["low"].is_null());
    CHECK(s[0]["latest"]["value"] == 21.5);
}

TEST_CASE("API serves series as JSON and as a CSV download", "[api]") {
    ApiFixture api;

    auto json = api.client->Get("/api/v1/series/temp?since=20");
    REQUIRE(json);
    REQUIRE(json->status == 200);
    const auto points = nlohmann::json::parse(json->body);
    REQUIRE(points.size() == 1);
    CHECK(points[0]["value"] == 21.5);

    auto csv = api.client->Get("/api/v1/series/temp?since=60&format=csv");
    REQUIRE(csv);
    CHECK(csv->status == 200);
    CHECK_THAT(csv->get_header_value("Content-Type"), StartsWith("text/csv"));
    CHECK_THAT(csv->get_header_value("Content-Disposition"), ContainsSubstring("temp.csv"));
    CHECK_THAT(csv->body, StartsWith("timestamp,value\n"));
    CHECK_THAT(csv->body, ContainsSubstring(",20.5\n"));
}

TEST_CASE("API rejects bad parameters with 400 and unknown sensors with 404", "[api]") {
    ApiFixture api;

    CHECK(api.client->Get("/api/v1/alerts?limit=abc")->status == 400);
    CHECK(api.client->Get("/api/v1/alerts?limit=0")->status == 400);
    CHECK(api.client->Get("/api/v1/series/temp?since=-5")->status == 400);
    CHECK(api.client->Get("/api/v1/series/temp?format=xml")->status == 400);
    CHECK(api.client->Get("/api/v1/series/nope")->status == 404);
}

TEST_CASE("API exposes alerts with kind, severity and limit details", "[api]") {
    ApiFixture api;
    auto res = api.client->Get("/api/v1/alerts?limit=5");
    REQUIRE(res);
    const auto alerts = nlohmann::json::parse(res->body);
    REQUIRE(alerts.size() == 1);
    CHECK(alerts[0]["kind"] == "limit");
    CHECK(alerts[0]["severity"] == "critical");
    CHECK(alerts[0]["limit"] == 90.0);
    CHECK(alerts[0]["direction"] == "high");
}

TEST_CASE("API exposes Prometheus metrics and health", "[api]") {
    ApiFixture api;

    auto metrics = api.client->Get("/metrics");
    REQUIRE(metrics);
    CHECK_THAT(metrics->get_header_value("Content-Type"), StartsWith("text/plain"));
    CHECK_THAT(metrics->body, ContainsSubstring("ironpulse_device_up{device=\"plc\"} 1"));

    auto health = api.client->Get("/healthz");
    REQUIRE(health);
    CHECK(nlohmann::json::parse(health->body)["status"] == "ok");
}

TEST_CASE("With a token configured, data endpoints require it", "[api][auth]") {
    ApiFixture api("t0ken");

    auto anonymous = api.client->Get("/api/v1/sensors");
    REQUIRE(anonymous);
    CHECK(anonymous->status == 401);
    CHECK(anonymous->get_header_value("WWW-Authenticate") == "Bearer");
    CHECK(api.client->Get("/metrics")->status == 401);

    auto wrong = api.client->Get("/api/v1/sensors", {{"Authorization", "Bearer nope"}});
    CHECK(wrong->status == 401);

    CHECK(api.client->Get("/api/v1/sensors", {{"Authorization", "Bearer t0ken"}})->status == 200);
    CHECK(api.client->Get("/api/v1/series/temp?format=csv&token=t0ken")->status == 200);

    // Bootstrap and health stay open; config tells the dashboard to ask.
    auto config = api.client->Get("/api/v1/config");
    REQUIRE(config->status == 200);
    CHECK(nlohmann::json::parse(config->body)["auth_required"] == true);
    CHECK(api.client->Get("/healthz")->status == 200);
}

TEST_CASE("constant_time_equals compares whole strings", "[api][auth]") {
    using ironpulse::api::constant_time_equals;
    CHECK(constant_time_equals("abc", "abc"));
    CHECK_FALSE(constant_time_equals("abc", "abd"));
    CHECK_FALSE(constant_time_equals("abc", "abcd"));
    CHECK_FALSE(constant_time_equals("", "a"));
    CHECK(constant_time_equals("", ""));
}

TEST_CASE("query_param extracts and decodes query parameters", "[api][ws]") {
    using ironpulse::api::query_param;
    CHECK(query_param("/live?token=abc", "token") == "abc");
    CHECK(query_param("/live?x=1&token=a%2Bb%3D", "token") == "a+b=");
    CHECK(query_param("/live", "token").empty());
    CHECK(query_param("/live?tokenx=1", "token").empty());
}

namespace {

/// Performs a raw WebSocket handshake and returns the status line.
std::string ws_handshake(std::uint16_t port, const std::string& target) {
    asio::io_context io;
    asio::ip::tcp::socket socket(io);
    socket.connect({asio::ip::address_v4::loopback(), port});
    const std::string request =
        "GET " + target +
        " HTTP/1.1\r\nHost: localhost\r\nUpgrade: websocket\r\nConnection: Upgrade\r\n"
        "sec-websocket-key: dGhlIHNhbXBsZSBub25jZQ==\r\nSec-WebSocket-Version: 13\r\n\r\n";
    asio::write(socket, asio::buffer(request));
    asio::streambuf response;
    asio::read_until(socket, response, "\r\n");
    std::istream stream(&response);
    std::string status_line;
    std::getline(stream, status_line);
    if (!status_line.empty() && status_line.back() == '\r') {
        status_line.pop_back();
    }
    return status_line;
}

}  // namespace

TEST_CASE("WebSocket handshake honours the token and lowercase headers", "[api][ws][auth]") {
    asio::io_context io;
    ironpulse::api::WsServer server(io, 0, "t0ken");
    server.start();
    std::thread runner([&io] { io.run(); });

    CHECK(ws_handshake(server.port(), "/live") == "HTTP/1.1 401 Unauthorized");
    CHECK(ws_handshake(server.port(), "/live?token=wrong") == "HTTP/1.1 401 Unauthorized");
    CHECK(ws_handshake(server.port(), "/live?token=t0ken") == "HTTP/1.1 101 Switching Protocols");

    server.stop();
    io.stop();
    runner.join();
}
