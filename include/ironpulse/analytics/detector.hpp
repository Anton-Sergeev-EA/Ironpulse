#pragma once

#include <cmath>
#include <deque>
#include <memory>
#include <string>

namespace ironpulse::analytics {

struct AnomalyResult {
    bool is_anomaly = false;
    double score = 0.0;       // strategy-specific magnitude (e.g. number of std deviations)
    double confidence = 0.0;  // normalized 0..1, for display/alerting
};

/// Strategy interface for anomaly detection algorithms. Each sensor gets its
/// own detector instance (they hold rolling state).
class AnomalyDetector {
public:
    virtual ~AnomalyDetector() = default;
    virtual AnomalyResult observe(double value) = 0;
    [[nodiscard]] virtual std::string name() const = 0;
};

/// Rolling z-score detector: flags samples that deviate more than
/// `threshold_sigma` standard deviations from the mean of the last
/// `window_size` samples.
class ZScoreDetector final : public AnomalyDetector {
public:
    explicit ZScoreDetector(std::size_t window_size = 60, double threshold_sigma = 3.0)
        : window_size_(window_size), threshold_sigma_(threshold_sigma) {}

    AnomalyResult observe(double value) override {
        AnomalyResult result;

        if (window_.size() >= 2) {
            const double mean = compute_mean();
            const double stddev = compute_stddev(mean);

            if (stddev > 1e-9) {
                const double z = std::abs(value - mean) / stddev;
                result.score = z;
                result.is_anomaly = z > threshold_sigma_;
                result.confidence = std::min(1.0, z / (threshold_sigma_ * 2.0));
            }
        }

        window_.push_back(value);
        if (window_.size() > window_size_) {
            window_.pop_front();
        }

        return result;
    }

    [[nodiscard]] std::string name() const override {
        return "zscore";
    }

private:
    [[nodiscard]] double compute_mean() const {
        double sum = 0.0;
        for (double v : window_) sum += v;
        return sum / static_cast<double>(window_.size());
    }

    [[nodiscard]] double compute_stddev(double mean) const {
        double sum_sq = 0.0;
        for (double v : window_) {
            const double diff = v - mean;
            sum_sq += diff * diff;
        }
        return std::sqrt(sum_sq / static_cast<double>(window_.size()));
    }

    std::size_t window_size_;
    double threshold_sigma_;
    std::deque<double> window_;
};

}  // namespace ironpulse::analytics
