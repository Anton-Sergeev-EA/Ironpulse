#include <catch2/catch_test_macros.hpp>
#include <chrono>
#include <vector>

#include "ironpulse/analytics/detector_factory.hpp"
#include "ironpulse/analytics/rule_engine.hpp"
#include "ironpulse/core/event_bus.hpp"

using ironpulse::analytics::RuleEngine;
using ironpulse::analytics::SensorRuleOptions;
using ironpulse::core::AlertKind;
using ironpulse::core::AnomalyEvent;
using ironpulse::core::EventBus;
using ironpulse::core::Severity;

namespace {

struct Harness {
    EventBus bus;
    RuleEngine engine{bus};
    std::vector<AnomalyEvent> alerts;
    std::chrono::system_clock::time_point t0 = std::chrono::system_clock::now();

    Harness() {
        bus.subscribe<AnomalyEvent>([this](const AnomalyEvent& e) { alerts.push_back(e); });
    }

    void feed(double value, int seconds_from_start) {
        engine.observe("s", value, t0 + std::chrono::seconds(seconds_from_start));
    }
};

}  // namespace

TEST_CASE("Crossing a high limit raises exactly one critical alert", "[rule_engine][limits]") {
    Harness h;
    SensorRuleOptions options;
    options.limits.high = 90.0;
    h.engine.register_sensor("s", {}, options);

    h.feed(80, 0);
    h.feed(95, 1);  // crosses
    h.feed(97, 2);  // still above: no repeat
    h.feed(99, 3);

    REQUIRE(h.alerts.size() == 1);
    const auto& a = h.alerts[0];
    CHECK(a.kind == AlertKind::limit);
    CHECK(a.severity == Severity::critical);
    CHECK(a.above_limit);
    CHECK(a.limit == 90.0);
    CHECK(a.value == 95.0);
}

TEST_CASE("A limit alert re-arms after the value returns inside the range", "[rule_engine][limits]") {
    Harness h;
    SensorRuleOptions options;
    options.limits.low = 10.0;
    options.limits.high = 90.0;
    h.engine.register_sensor("s", {}, options);

    h.feed(95, 0);  // high
    h.feed(50, 1);  // back inside
    h.feed(5, 2);   // low

    REQUIRE(h.alerts.size() == 2);
    CHECK(h.alerts[0].above_limit);
    CHECK_FALSE(h.alerts[1].above_limit);
    CHECK(h.alerts[1].limit == 10.0);
}

TEST_CASE("Cooldown suppresses a value chattering around its limit", "[rule_engine][limits]") {
    Harness h;
    SensorRuleOptions options;
    options.limits.high = 90.0;
    options.cooldown = std::chrono::seconds(60);
    h.engine.register_sensor("s", {}, options);

    h.feed(91, 0);   // alert
    h.feed(89, 1);   // inside
    h.feed(91, 2);   // crosses again within the cooldown: suppressed
    h.feed(89, 70);  // inside
    h.feed(91, 71);  // crosses after the cooldown: alert

    CHECK(h.alerts.size() == 2);
}

TEST_CASE("Cooldown limits statistical alerts during a sustained excursion", "[rule_engine][cooldown]") {
    Harness h;
    std::vector<std::unique_ptr<ironpulse::analytics::AnomalyDetector>> strategies;
    strategies.push_back(std::make_unique<ironpulse::analytics::ZScoreDetector>(20, 3.0));
    SensorRuleOptions options;
    options.votes_required = 1;
    options.cooldown = std::chrono::seconds(30);
    h.engine.register_sensor("s", std::move(strategies), options);

    for (int i = 0; i < 25; ++i) {
        h.feed(100.0 + (i % 2 == 0 ? 0.1 : -0.1), i);
    }
    h.feed(500, 25);  // anomaly
    h.feed(100, 26);
    h.feed(600, 27);  // also anomalous, but within the cooldown

    REQUIRE(h.alerts.size() == 1);
    CHECK(h.alerts[0].kind == AlertKind::statistical);
    CHECK(h.alerts[0].severity == Severity::warning);
    CHECK(h.alerts[0].value == 500.0);
}

TEST_CASE("register_sensor builds detectors from configuration", "[rule_engine][factory]") {
    ironpulse::core::SensorConfig sensor;
    sensor.id = "s";
    ironpulse::core::DetectorConfig cusum;
    cusum.type = "cusum";
    cusum.mean = 100.0;
    cusum.stddev = 1.0;
    cusum.slack = 0.5;
    cusum.threshold = 5.0;
    sensor.detection.detectors = {cusum};
    sensor.detection.votes_required = 1;

    Harness h;
    ironpulse::analytics::register_sensor(h.engine, sensor);

    // A sustained +2σ shift is invisible to a single-sample check but
    // accumulates in CUSUM: (2 - 0.5) per sample crosses 5 on the 4th.
    for (int i = 0; i < 4; ++i) {
        h.feed(102.0, i);
    }
    REQUIRE(h.alerts.size() == 1);
    CHECK(h.alerts[0].detector_name == "cusum");

    ironpulse::core::DetectorConfig unknown;
    unknown.type = "unknown";
    CHECK_THROWS(ironpulse::analytics::make_detector(unknown));
}
