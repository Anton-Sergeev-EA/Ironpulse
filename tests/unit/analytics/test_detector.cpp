#include <catch2/catch_test_macros.hpp>

#include "ironpulse/analytics/detector.hpp"

using ironpulse::analytics::ZScoreDetector;

TEST_CASE("ZScoreDetector does not flag a stable signal", "[detector]") {
    ZScoreDetector detector(/*window_size=*/20, /*threshold_sigma=*/3.0);

    bool any_anomaly = false;
    for (int i = 0; i < 50; ++i) {
        auto result = detector.observe(100.0);  // constant signal
        any_anomaly |= result.is_anomaly;
    }

    CHECK_FALSE(any_anomaly);
}

TEST_CASE("ZScoreDetector flags a sharp spike after a stable baseline", "[detector]") {
    ZScoreDetector detector(/*window_size=*/20, /*threshold_sigma=*/3.0);

    for (int i = 0; i < 20; ++i) {
        detector.observe(100.0 + (i % 2 == 0 ? 0.1 : -0.1));  // tiny stable noise
    }

    auto result = detector.observe(500.0);  // sharp spike
    CHECK(result.is_anomaly);
    CHECK(result.score > 3.0);
}

TEST_CASE("ZScoreDetector needs at least two samples before scoring", "[detector]") {
    ZScoreDetector detector;
    auto first = detector.observe(42.0);
    CHECK_FALSE(first.is_anomaly);
    CHECK(first.score == 0.0);
}

TEST_CASE("ZScoreDetector does not vote before half its window is filled", "[detector]") {
    ZScoreDetector detector(/*window_size=*/60, /*threshold_sigma=*/3.0);  // warm-up: 30 samples

    bool any_anomaly = false;
    for (int i = 0; i < 29; ++i) {
        any_anomaly |= detector.observe(i % 2 == 0 ? 100.0 : 101.0).is_anomaly;
    }
    any_anomaly |= detector.observe(500.0).is_anomaly;  // 30th sample: still warming up
    CHECK_FALSE(any_anomaly);

    ZScoreDetector warmed(/*window_size=*/60, /*threshold_sigma=*/3.0);
    for (int i = 0; i < 30; ++i) {
        warmed.observe(i % 2 == 0 ? 100.0 : 101.0);
    }
    CHECK(warmed.observe(500.0).is_anomaly);  // same spike, once warmed up
}
