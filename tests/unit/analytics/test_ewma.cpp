#include <catch2/catch_test_macros.hpp>

#include "ironpulse/analytics/strategies/ewma.hpp"

using ironpulse::analytics::EwmaDetector;

TEST_CASE("EwmaDetector does not flag a stable signal", "[detector][ewma]") {
    EwmaDetector detector(0.2, 3.0);

    bool any_anomaly = false;
    for (int i = 0; i < 50; ++i) {
        auto result = detector.observe(100.0 + (i % 2 == 0 ? 0.05 : -0.05));
        any_anomaly |= result.is_anomaly;
    }

    CHECK_FALSE(any_anomaly);
}

TEST_CASE("EwmaDetector flags a sharp spike after a stable baseline", "[detector][ewma]") {
    EwmaDetector detector(0.2, 3.0);

    for (int i = 0; i < 30; ++i) {
        detector.observe(100.0 + (i % 2 == 0 ? 0.05 : -0.05));
    }

    auto result = detector.observe(500.0);
    CHECK(result.is_anomaly);
}

TEST_CASE("EwmaDetector's first observation is never an anomaly", "[detector][ewma]") {
    EwmaDetector detector;
    auto result = detector.observe(42.0);
    CHECK_FALSE(result.is_anomaly);
}

TEST_CASE("EwmaDetector reports its name", "[detector][ewma]") {
    EwmaDetector detector;
    CHECK(detector.name() == "ewma");
}

TEST_CASE("EwmaDetector stays silent while its variance estimate warms up", "[detector][ewma]") {
    EwmaDetector detector(0.2, 3.0);  // warm-up: ceil(3 / 0.2) = 15 samples

    bool any_anomaly = false;
    any_anomaly |= detector.observe(100.0).is_anomaly;
    any_anomaly |= detector.observe(100.1).is_anomaly;
    any_anomaly |= detector.observe(130.0).is_anomaly;  // would be a huge z-score on a 2-sample variance
    CHECK_FALSE(any_anomaly);
}
