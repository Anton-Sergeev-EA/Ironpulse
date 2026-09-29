#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include "ironpulse/core/metrics.hpp"

using Catch::Matchers::ContainsSubstring;
using ironpulse::core::Metrics;

TEST_CASE("Metrics renders described counters and gauges in Prometheus format", "[metrics]") {
    Metrics m;
    m.describe("ironpulse_readings_total", Metrics::Type::counter, "Sensor readings received.");
    m.increment("ironpulse_readings_total", {{"sensor", "t1"}});
    m.increment("ironpulse_readings_total", {{"sensor", "t1"}}, 2);
    m.set("ironpulse_device_up", {{"device", "plc"}}, 1);

    const std::string out = m.render();

    CHECK_THAT(out, ContainsSubstring("# HELP ironpulse_readings_total Sensor readings received.\n"));
    CHECK_THAT(out, ContainsSubstring("# TYPE ironpulse_readings_total counter\n"));
    CHECK_THAT(out, ContainsSubstring("ironpulse_readings_total{sensor=\"t1\"} 3\n"));
    CHECK_THAT(out, ContainsSubstring("ironpulse_device_up{device=\"plc\"} 1\n"));
    CHECK(m.value("ironpulse_readings_total", {{"sensor", "t1"}}) == 3.0);
}

TEST_CASE("Metrics escapes label values", "[metrics]") {
    CHECK(Metrics::escape_label_value("a\"b\\c\nd") == "a\\\"b\\\\c\\nd");

    Metrics m;
    m.set("g", {{"unit", "°C \"x\""}}, 2.5);
    CHECK_THAT(m.render(), ContainsSubstring("g{unit=\"°C \\\"x\\\"\"} 2.5\n"));
}

TEST_CASE("Metrics omits families that have no series yet", "[metrics]") {
    Metrics m;
    m.describe("ironpulse_alerts_total", Metrics::Type::counter, "Alerts.");
    CHECK(m.render().empty());
}
