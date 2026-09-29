// End-to-end pipeline test: a fake Modbus device -> DevicePoller (real TCP,
// batched reads, decoding, scaling) -> EventBus -> RuleEngine -> alerts.
// This is the "integration test: full pipeline from a mock device to a
// published alert" item from the roadmap.

#include <asio.hpp>
#include <atomic>
#include <bit>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <chrono>
#include <condition_variable>
#include <map>
#include <mutex>
#include <thread>

#include "../support/fake_modbus_server.hpp"
#include "ironpulse/analytics/detector_factory.hpp"
#include "ironpulse/analytics/rule_engine.hpp"
#include "ironpulse/core/event_bus.hpp"
#include "ironpulse/core/events.hpp"
#include "ironpulse/protocol/device_poller.hpp"

using Catch::Matchers::WithinAbs;
using ironpulse::core::AnomalyEvent;
using ironpulse::core::DeviceStatusEvent;
using ironpulse::core::PollErrorEvent;
using ironpulse::core::SensorReadingEvent;
using ironpulse::testing::FakeModbusServer;

namespace {

/// Collects bus events and lets the test wait for a condition on them.
class Recorder {
public:
    explicit Recorder(ironpulse::core::EventBus& bus) {
        bus.subscribe<SensorReadingEvent>([this](const SensorReadingEvent& e) {
            std::lock_guard lock(mutex_);
            latest_[e.sensor_id] = e.value;
            ++readings_;
            cv_.notify_all();
        });
        bus.subscribe<AnomalyEvent>([this](const AnomalyEvent& e) {
            std::lock_guard lock(mutex_);
            alerts_.push_back(e);
            cv_.notify_all();
        });
        bus.subscribe<DeviceStatusEvent>([this](const DeviceStatusEvent& e) {
            std::lock_guard lock(mutex_);
            statuses_.push_back(e.online);
            cv_.notify_all();
        });
        bus.subscribe<PollErrorEvent>([this](const PollErrorEvent&) {
            std::lock_guard lock(mutex_);
            ++errors_;
            cv_.notify_all();
        });
    }

    template <typename Predicate>
    bool wait_for(Predicate predicate, std::chrono::milliseconds timeout = std::chrono::seconds(5)) {
        std::unique_lock lock(mutex_);
        return cv_.wait_for(lock, timeout, [&] { return predicate(*this); });
    }

    int readings() {
        std::lock_guard lock(mutex_);
        return readings_;
    }

    // Read directly only from inside wait_for (mutex held) or after the
    // poller has been stopped.
    std::map<std::string, double> latest_;
    std::vector<AnomalyEvent> alerts_;
    std::vector<bool> statuses_;
    int readings_ = 0;
    int errors_ = 0;

private:
    std::mutex mutex_;
    std::condition_variable cv_;
};

/// Runs an io_context on a background thread for the test's lifetime.
class IoRunner {
public:
    IoRunner() : guard_(asio::make_work_guard(io)), thread_([this] { io.run(); }) {}
    ~IoRunner() {
        guard_.reset();
        io.stop();
        thread_.join();
    }
    asio::io_context io;

private:
    asio::executor_work_guard<asio::io_context::executor_type> guard_;
    std::thread thread_;
};

ironpulse::core::ModbusDeviceConfig make_device(std::uint16_t port) {
    ironpulse::core::ModbusDeviceConfig device;
    device.id = "plc_1";
    device.host = "127.0.0.1";
    device.port = port;
    device.unit_id = 1;
    device.poll_interval_ms = 50;
    device.timeout_ms = 300;

    ironpulse::core::SensorConfig temperature;  // float32, big-endian words, holding 0-1
    temperature.id = "temp";
    temperature.data_type = ironpulse::core::DataType::float32;
    temperature.address = 0;
    temperature.limits.high = 90.0;
    temperature.detection.detectors.clear();  // limits only: deterministic

    ironpulse::core::SensorConfig pressure;  // int16 x 0.1, holding 2 — same block as temp
    pressure.id = "pressure";
    pressure.data_type = ironpulse::core::DataType::int16;
    pressure.address = 2;
    pressure.scale = 0.1;
    pressure.detection.detectors.clear();

    ironpulse::core::SensorConfig flow;  // uint32 little word order, input 10-11
    flow.id = "flow";
    flow.register_type = ironpulse::core::RegisterType::input;
    flow.data_type = ironpulse::core::DataType::uint32;
    flow.word_order = ironpulse::core::WordOrder::little;
    flow.address = 10;
    flow.detection.detectors.clear();

    device.sensors = {temperature, pressure, flow};
    return device;
}

void set_float(FakeModbusServer& server, std::uint16_t address, float value) {
    const auto bits = std::bit_cast<std::uint32_t>(value);
    server.set_holding(address, static_cast<std::uint16_t>(bits >> 16));
    server.set_holding(static_cast<std::uint16_t>(address + 1), static_cast<std::uint16_t>(bits & 0xFFFF));
}

}  // namespace

TEST_CASE("Pipeline decodes several sensors from one device in batched reads", "[integration][pipeline]") {
    FakeModbusServer server;
    set_float(server, 0, 72.5F);
    server.set_holding(2, static_cast<std::uint16_t>(-123));  // -12.3 after scale
    server.set_input(10, 0x0001);                             // low word first:
    server.set_input(11, 0x0002);                             // 0x0002'0001 = 131073

    ironpulse::core::EventBus bus;
    Recorder recorder(bus);
    IoRunner runner;

    auto poller = ironpulse::protocol::DevicePoller::create(runner.io, make_device(server.port()), bus);
    poller->start();

    REQUIRE(recorder.wait_for([](const Recorder& r) { return r.latest_.size() == 3; }));
    REQUIRE(recorder.wait_for([](const Recorder& r) { return !r.statuses_.empty(); }));
    poller->stop();

    CHECK_THAT(recorder.latest_.at("temp"), WithinAbs(72.5, 1e-6));
    CHECK_THAT(recorder.latest_.at("pressure"), WithinAbs(-12.3, 1e-9));
    CHECK_THAT(recorder.latest_.at("flow"), WithinAbs(131073.0, 1e-9));
    CHECK(recorder.statuses_.front() == true);
}

TEST_CASE("Pipeline raises one critical alert when a sensor crosses its limit", "[integration][pipeline]") {
    FakeModbusServer server;
    set_float(server, 0, 70.0F);

    ironpulse::core::EventBus bus;
    Recorder recorder(bus);
    ironpulse::analytics::RuleEngine engine(bus);
    const auto device = make_device(server.port());
    for (const auto& sensor : device.sensors) {
        ironpulse::analytics::register_sensor(engine, sensor);
    }
    bus.subscribe<SensorReadingEvent>(
        [&engine](const SensorReadingEvent& r) { engine.observe(r.sensor_id, r.value, r.timestamp); });

    IoRunner runner;
    auto poller = ironpulse::protocol::DevicePoller::create(runner.io, device, bus);
    poller->start();

    REQUIRE(recorder.wait_for([](const Recorder& r) { return r.readings_ >= 3; }));
    set_float(server, 0, 120.0F);  // above the 90 °C limit
    REQUIRE(recorder.wait_for([](const Recorder& r) { return !r.alerts_.empty(); }));

    // Several more polls while the value stays high must not repeat the alert.
    const int readings_at_alert = recorder.readings();
    REQUIRE(recorder.wait_for([&](const Recorder& r) { return r.readings_ >= readings_at_alert + 9; }));
    poller->stop();

    REQUIRE(recorder.alerts_.size() == 1);
    const auto& alert = recorder.alerts_.front();
    CHECK(alert.sensor_id == "temp");
    CHECK(alert.kind == ironpulse::core::AlertKind::limit);
    CHECK(alert.severity == ironpulse::core::Severity::critical);
    CHECK(alert.above_limit);
    CHECK_THAT(alert.value, WithinAbs(120.0, 1e-6));
}

TEST_CASE("Pipeline marks a device offline when it stops answering, and recovers",
          "[integration][pipeline]") {
    FakeModbusServer server;
    set_float(server, 0, 50.0F);

    ironpulse::core::EventBus bus;
    Recorder recorder(bus);
    IoRunner runner;
    auto poller = ironpulse::protocol::DevicePoller::create(runner.io, make_device(server.port()), bus);
    poller->start();

    REQUIRE(recorder.wait_for([](const Recorder& r) { return !r.statuses_.empty() && r.statuses_.back(); }));

    // Accepts the connection but never answers: only the request timeout
    // can detect this.
    server.set_silent(true);
    REQUIRE(
        recorder.wait_for([](const Recorder& r) { return r.statuses_.size() >= 2 && !r.statuses_.back(); }));
    CHECK(recorder.wait_for([](const Recorder& r) { return r.errors_ >= 1; }));

    server.set_silent(false);
    REQUIRE(
        recorder.wait_for([](const Recorder& r) { return r.statuses_.size() >= 3 && r.statuses_.back(); }));
    poller->stop();
}

TEST_CASE("Pipeline reports an unreachable device as offline", "[integration][pipeline]") {
    std::uint16_t closed_port = 0;
    {
        FakeModbusServer server;  // grab a free port, then release it
        closed_port = server.port();
    }

    ironpulse::core::EventBus bus;
    Recorder recorder(bus);
    IoRunner runner;
    auto poller = ironpulse::protocol::DevicePoller::create(runner.io, make_device(closed_port), bus);
    poller->start();

    REQUIRE(recorder.wait_for([](const Recorder& r) { return !r.statuses_.empty(); }));
    poller->stop();
    CHECK(recorder.statuses_.front() == false);
}
