#include <catch2/catch_test_macros.hpp>

#include "ironpulse/analytics/rule_engine.hpp"
#include "ironpulse/analytics/strategies/ewma.hpp"
#include "ironpulse/core/event_bus.hpp"

using ironpulse::analytics::EwmaDetector;
using ironpulse::analytics::RuleEngine;
using ironpulse::analytics::ZScoreDetector;
using ironpulse::core::AnomalyEvent;
using ironpulse::core::EventBus;

TEST_CASE("RuleEngine publishes nothing for an unregistered sensor", "[rule_engine]") {
    EventBus bus;
    RuleEngine engine(bus);
    bool called = false;
    bus.subscribe<AnomalyEvent>([&](const AnomalyEvent&) { called = true; });

    engine.observe("unknown_sensor", 500.0, std::chrono::system_clock::now());

    CHECK_FALSE(called);
}

TEST_CASE("RuleEngine requires the configured vote quorum before publishing", "[rule_engine]") {
    EventBus bus;
    RuleEngine engine(bus);

    std::vector<AnomalyEvent> received;
    bus.subscribe<AnomalyEvent>([&](const AnomalyEvent& e) { received.push_back(e); });

    std::vector<std::unique_ptr<ironpulse::analytics::AnomalyDetector>> strategies;
    strategies.push_back(std::make_unique<ZScoreDetector>(20, 3.0));
    strategies.push_back(std::make_unique<EwmaDetector>(0.2, 3.0));
    // Require BOTH strategies to agree.
    engine.register_sensor("sensor_a", std::move(strategies), /*votes_required=*/2);

    const auto now = std::chrono::system_clock::now();
    // Establish a stable baseline for both detectors.
    for (int i = 0; i < 30; ++i) {
        engine.observe("sensor_a", 100.0 + (i % 2 == 0 ? 0.05 : -0.05), now);
    }
    CHECK(received.empty());

    // A large sustained spike should eventually get both detectors to agree.
    engine.observe("sensor_a", 500.0, now);

    REQUIRE(received.size() == 1);
    CHECK(received[0].sensor_id == "sensor_a");
    CHECK(received[0].confidence > 0.0);
}

TEST_CASE("RuleEngine with votes_required=1 fires on a single detector's signal", "[rule_engine]") {
    EventBus bus;
    RuleEngine engine(bus);

    int alert_count = 0;
    bus.subscribe<AnomalyEvent>([&](const AnomalyEvent&) { ++alert_count; });

    std::vector<std::unique_ptr<ironpulse::analytics::AnomalyDetector>> strategies;
    strategies.push_back(std::make_unique<ZScoreDetector>(20, 3.0));
    engine.register_sensor("sensor_b", std::move(strategies), /*votes_required=*/1);

    const auto now = std::chrono::system_clock::now();
    for (int i = 0; i < 25; ++i) {
        engine.observe("sensor_b", 100.0 + (i % 2 == 0 ? 0.05 : -0.05), now);
    }
    engine.observe("sensor_b", 999.0, now);

    CHECK(alert_count == 1);
}

TEST_CASE("RuleEngine default votes_required equals the strategy count", "[rule_engine]") {
    EventBus bus;
    RuleEngine engine(bus);

    int alert_count = 0;
    bus.subscribe<AnomalyEvent>([&](const AnomalyEvent&) { ++alert_count; });

    std::vector<std::unique_ptr<ironpulse::analytics::AnomalyDetector>> strategies;
    strategies.push_back(std::make_unique<ZScoreDetector>(20, 3.0));
    strategies.push_back(std::make_unique<EwmaDetector>(0.2, 3.0));
    engine.register_sensor("sensor_c", std::move(strategies));  // votes_required defaults to 2

    const auto now = std::chrono::system_clock::now();
    for (int i = 0; i < 30; ++i) {
        engine.observe("sensor_c", 100.0 + (i % 2 == 0 ? 0.05 : -0.05), now);
    }
    engine.observe("sensor_c", 800.0, now);

    CHECK(alert_count == 1);
}
