#include <catch2/catch_test_macros.hpp>
#include <string>

#include "ironpulse/core/event_bus.hpp"

using ironpulse::core::EventBus;

namespace {
struct SensorReading {
    std::string sensor_id;
    double value;
};

struct AlertRaised {
    std::string message;
};
}  // namespace

TEST_CASE("EventBus delivers published events to subscribers of the same type", "[event_bus]") {
    EventBus bus;
    SensorReading received{};
    bool called = false;

    bus.subscribe<SensorReading>([&](const SensorReading& e) {
        received = e;
        called = true;
    });

    bus.publish(SensorReading{"temp_01", 42.5});

    REQUIRE(called);
    CHECK(received.sensor_id == "temp_01");
    CHECK(received.value == 42.5);
}

TEST_CASE("EventBus does not cross-deliver events of different types", "[event_bus]") {
    EventBus bus;
    bool sensor_called = false;
    bool alert_called = false;

    bus.subscribe<SensorReading>([&](const SensorReading&) { sensor_called = true; });
    bus.subscribe<AlertRaised>([&](const AlertRaised&) { alert_called = true; });

    bus.publish(AlertRaised{"threshold exceeded"});

    CHECK_FALSE(sensor_called);
    CHECK(alert_called);
}

TEST_CASE("EventBus stops calling a handler after unsubscribe", "[event_bus]") {
    EventBus bus;
    int call_count = 0;

    auto id = bus.subscribe<SensorReading>([&](const SensorReading&) { ++call_count; });
    bus.publish(SensorReading{"a", 1.0});
    bus.unsubscribe<SensorReading>(id);
    bus.publish(SensorReading{"a", 2.0});

    CHECK(call_count == 1);
}

TEST_CASE("EventBus supports multiple subscribers for the same event", "[event_bus]") {
    EventBus bus;
    int first = 0;
    int second = 0;

    bus.subscribe<SensorReading>([&](const SensorReading&) { ++first; });
    bus.subscribe<SensorReading>([&](const SensorReading&) { ++second; });
    bus.publish(SensorReading{"a", 1.0});

    CHECK(first == 1);
    CHECK(second == 1);
}
