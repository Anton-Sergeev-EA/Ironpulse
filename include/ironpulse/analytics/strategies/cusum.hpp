#pragma once

#include <algorithm>
#include <cmath>
#include <string>

#include "ironpulse/analytics/detector.hpp"

namespace ironpulse::analytics {

/// Two-sided CUSUM (cumulative sum) control-chart detector.
///
/// Accumulates evidence of a sustained shift away from a reference mean,
/// rather than reacting to single-sample noise. This is the strategy most
/// suited to catching slow degradation (e.g. a bearing's vibration
/// creeping upward over hours) that a single-sample z-score would miss
/// until it was already severe.
///
/// `slack` (k) is the allowed drift per sample before it counts against
/// the cumulative sum, typically ~0.5 * expected shift in std-devs.
/// `threshold` (h) is the cumulative sum at which an alarm fires,
/// typically 4-5 std-devs.
class CusumDetector final : public AnomalyDetector {
public:
    explicit CusumDetector(double reference_mean,
                           double reference_stddev,
                           double slack = 0.5,
                           double threshold = 5.0)
        : mean_(reference_mean),
          stddev_(std::max(reference_stddev, 1e-9)),
          slack_(slack),
          threshold_(threshold) {}

    AnomalyResult observe(double value) override {
        const double standardized = (value - mean_) / stddev_;

        cumulative_high_ = std::max(0.0, cumulative_high_ + standardized - slack_);
        cumulative_low_ = std::max(0.0, cumulative_low_ - standardized - slack_);

        const double drive = std::max(cumulative_high_, cumulative_low_);

        AnomalyResult result;
        result.score = drive;
        result.is_anomaly = drive > threshold_;
        result.confidence = std::min(1.0, drive / (threshold_ * 1.5));

        if (result.is_anomaly) {
            // Reset after firing so a single sustained excursion doesn't
            // keep re-triggering every sample indefinitely.
            cumulative_high_ = 0.0;
            cumulative_low_ = 0.0;
        }

        return result;
    }

    [[nodiscard]] std::string name() const override {
        return "cusum";
    }

private:
    double mean_;
    double stddev_;
    double slack_;
    double threshold_;
    double cumulative_high_ = 0.0;
    double cumulative_low_ = 0.0;
};

}  // namespace ironpulse::analytics
