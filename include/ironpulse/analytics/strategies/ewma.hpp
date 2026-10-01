#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <string>

#include "ironpulse/analytics/detector.hpp"

namespace ironpulse::analytics {

/// Exponentially-weighted moving average detector.
///
/// Tracks a running mean and variance with exponential decay (weight
/// `alpha` on the newest sample), and flags values that fall more than
/// `threshold_sigma` estimated standard deviations from that mean.
///
/// Reacts faster than a fixed rolling window to genuine trend shifts
/// (e.g. a slow temperature drift), at the cost of being noisier on
/// short transients — pairs well with ZScoreDetector in a RuleEngine
/// quorum to cancel out each one's false positives.
///
/// The variance estimate starts at zero, so the detector only votes after
/// ceil(3 / alpha) samples, once the estimate has converged — before that
/// almost any value looks like many standard deviations away.
class EwmaDetector final : public AnomalyDetector {
public:
    explicit EwmaDetector(double alpha = 0.2, double threshold_sigma = 3.0)
        : alpha_(alpha),
          threshold_sigma_(threshold_sigma),
          warmup_(static_cast<std::size_t>(std::ceil(3.0 / std::max(alpha, 1e-3)))) {}

    AnomalyResult observe(double value) override {
        AnomalyResult result;

        if (initialized_) {
            const double deviation = value - mean_;
            const double stddev = std::sqrt(variance_);

            if (stddev > 1e-9 && samples_ >= warmup_) {
                const double z = std::abs(deviation) / stddev;
                result.score = z;
                result.is_anomaly = z > threshold_sigma_;
                result.confidence = std::min(1.0, z / (threshold_sigma_ * 2.0));
            }

            // Exponential update of mean and variance (Welford-style EWMA variance).
            mean_ += alpha_ * deviation;
            variance_ = (1.0 - alpha_) * (variance_ + alpha_ * deviation * deviation);
        } else {
            mean_ = value;
            variance_ = 0.0;
            initialized_ = true;
        }
        ++samples_;

        return result;
    }

    [[nodiscard]] std::string name() const override {
        return "ewma";
    }

private:
    double alpha_;
    double threshold_sigma_;
    std::size_t warmup_;
    std::size_t samples_ = 0;
    bool initialized_ = false;
    double mean_ = 0.0;
    double variance_ = 0.0;
};

}  // namespace ironpulse::analytics
