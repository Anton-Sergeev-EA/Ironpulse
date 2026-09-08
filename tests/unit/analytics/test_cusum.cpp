#include <catch2/catch_test_macros.hpp>

#include "ironpulse/analytics/strategies/cusum.hpp"

using ironpulse::analytics::CusumDetector;

TEST_CASE("CusumDetector does not flag noise around the reference mean", "[detector][cusum]") {
    CusumDetector detector(/*reference_mean=*/100.0,
                           /*reference_stddev=*/2.0,
                           /*slack=*/0.5,
                           /*threshold=*/5.0);

    bool any_anomaly = false;
    for (int i = 0; i < 100; ++i) {
        auto result = detector.observe(100.0 + (i % 2 == 0 ? 1.0 : -1.0));
        any_anomaly |= result.is_anomaly;
    }

    CHECK_FALSE(any_anomaly);
}

TEST_CASE("CusumDetector flags a sustained upward shift", "[detector][cusum]") {
    CusumDetector detector(/*reference_mean=*/100.0,
                           /*reference_stddev=*/2.0,
                           /*slack=*/0.5,
                           /*threshold=*/5.0);

    bool flagged = false;
    for (int i = 0; i < 20; ++i) {
        auto result = detector.observe(106.0);  // sustained +3 sigma shift
        if (result.is_anomaly) {
            flagged = true;
            break;
        }
    }

    CHECK(flagged);
}

TEST_CASE("CusumDetector flags a sustained downward shift", "[detector][cusum]") {
    CusumDetector detector(/*reference_mean=*/100.0,
                           /*reference_stddev=*/2.0,
                           /*slack=*/0.5,
                           /*threshold=*/5.0);

    bool flagged = false;
    for (int i = 0; i < 20; ++i) {
        auto result = detector.observe(94.0);  // sustained -3 sigma shift
        if (result.is_anomaly) {
            flagged = true;
            break;
        }
    }

    CHECK(flagged);
}

TEST_CASE("CusumDetector reports its name", "[detector][cusum]") {
    CusumDetector detector(100.0, 2.0);
    CHECK(detector.name() == "cusum");
}
