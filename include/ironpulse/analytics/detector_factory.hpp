#pragma once

#include <memory>
#include <stdexcept>
#include <vector>

#include "ironpulse/analytics/detector.hpp"
#include "ironpulse/analytics/rule_engine.hpp"
#include "ironpulse/analytics/strategies/cusum.hpp"
#include "ironpulse/analytics/strategies/ewma.hpp"
#include "ironpulse/core/config.hpp"

namespace ironpulse::analytics {

/// Builds a detection strategy from its configuration. The config loader
/// has already validated the type and parameters.
[[nodiscard]] inline std::unique_ptr<AnomalyDetector> make_detector(const core::DetectorConfig& cfg) {
    if (cfg.type == "zscore") {
        return std::make_unique<ZScoreDetector>(cfg.window, cfg.threshold);
    }
    if (cfg.type == "ewma") {
        return std::make_unique<EwmaDetector>(cfg.alpha, cfg.threshold);
    }
    if (cfg.type == "cusum") {
        return std::make_unique<CusumDetector>(cfg.mean, cfg.stddev, cfg.slack, cfg.threshold);
    }
    throw std::invalid_argument("unknown detector type: " + cfg.type);
}

/// Registers a configured sensor (its detectors, quorum, cooldown and
/// limits) with the rule engine.
inline void register_sensor(RuleEngine& engine, const core::SensorConfig& sensor) {
    std::vector<std::unique_ptr<AnomalyDetector>> strategies;
    strategies.reserve(sensor.detection.detectors.size());
    for (const auto& detector : sensor.detection.detectors) {
        strategies.push_back(make_detector(detector));
    }
    SensorRuleOptions options;
    options.votes_required = sensor.detection.votes_required;
    options.cooldown = std::chrono::seconds(sensor.detection.cooldown_seconds);
    options.limits = sensor.limits;
    engine.register_sensor(sensor.id, std::move(strategies), options);
}

}  // namespace ironpulse::analytics
