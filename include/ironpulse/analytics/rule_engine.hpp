#pragma once

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "ironpulse/analytics/detector.hpp"
#include "ironpulse/core/config.hpp"
#include "ironpulse/core/event_bus.hpp"
#include "ironpulse/core/events.hpp"

namespace ironpulse::analytics {

/// Per-sensor alerting options beyond the detector list itself.
struct SensorRuleOptions {
    /// How many strategies must agree; 0 means "all of them".
    std::size_t votes_required = 0;
    /// Minimum time between two alerts of the same kind for this sensor.
    std::chrono::seconds cooldown{0};
    /// Hard limits; crossing one raises a critical alert.
    core::Limits limits;
};

/// Turns readings into alerts.
///
/// Two independent mechanisms run on every reading:
///
/// * Statistical detection — multiple AnomalyDetector strategies vote, and
///   an alert (severity: warning) is raised only when at least
///   `votes_required` agree. This is what turns "a detector fired" into
///   "an alert worth waking someone up for" — a lone z-score blip on noisy
///   sensor data is common; z-score *and* CUSUM *and* EWMA agreeing is not.
///
/// * Hard limits — a reading outside the configured low/high range raises
///   a critical alert. Limit alerts are edge-triggered: one alert when the
///   value leaves the allowed range, not one per sample while it stays out.
///
/// A per-sensor cooldown applies to each mechanism separately, so a
/// sustained excursion produces one notification instead of a storm.
///
/// Thread-safe: `observe()` may be called concurrently for different (or
/// the same) sensors from multiple io_context worker threads.
class RuleEngine {
public:
    explicit RuleEngine(core::EventBus& bus) : bus_(bus) {}

    /// Registers a sensor with its detection strategies. `votes_required`
    /// defaults to requiring every registered strategy to agree.
    void register_sensor(const std::string& sensor_id,
                         std::vector<std::unique_ptr<AnomalyDetector>> strategies,
                         std::size_t votes_required = 0) {
        SensorRuleOptions options;
        options.votes_required = votes_required;
        register_sensor(sensor_id, std::move(strategies), options);
    }

    void register_sensor(const std::string& sensor_id,
                         std::vector<std::unique_ptr<AnomalyDetector>> strategies,
                         const SensorRuleOptions& options) {
        std::lock_guard lock(mutex_);
        SensorRule rule;
        rule.strategies = std::move(strategies);
        rule.votes_required = options.votes_required == 0 ? rule.strategies.size() : options.votes_required;
        rule.cooldown = options.cooldown;
        rule.limits = options.limits;
        rules_[sensor_id] = std::move(rule);
    }

    /// Feeds a new reading through the sensor's limit check and detection
    /// strategies, publishing a core::AnomalyEvent on the EventBus for
    /// each alert raised. No-op (but harmless) for unregistered sensors.
    void observe(const std::string& sensor_id,
                 double value,
                 std::chrono::system_clock::time_point timestamp) {
        std::vector<core::AnomalyEvent> alerts;
        {
            std::lock_guard lock(mutex_);
            auto it = rules_.find(sensor_id);
            if (it == rules_.end()) {
                return;
            }
            SensorRule& rule = it->second;
            if (auto alert = check_limits(sensor_id, rule, value, timestamp)) {
                alerts.push_back(std::move(*alert));
            }
            if (auto alert = run_detectors(sensor_id, rule, value, timestamp)) {
                alerts.push_back(std::move(*alert));
            }
        }
        // Published outside the lock: subscribers (storage, notifications,
        // WebSocket broadcast) must never be able to deadlock the engine.
        for (const auto& alert : alerts) {
            bus_.publish(alert);
        }
    }

private:
    enum class LimitState { inside, above, below };

    struct SensorRule {
        std::vector<std::unique_ptr<AnomalyDetector>> strategies;
        std::size_t votes_required = 1;
        std::chrono::seconds cooldown{0};
        core::Limits limits;
        LimitState limit_state = LimitState::inside;
        std::optional<std::chrono::system_clock::time_point> last_limit_alert;
        std::optional<std::chrono::system_clock::time_point> last_statistical_alert;
    };

    static bool cooled_down(const std::optional<std::chrono::system_clock::time_point>& last,
                            std::chrono::seconds cooldown,
                            std::chrono::system_clock::time_point now) {
        return !last || now - *last >= cooldown;
    }

    static std::string format_number(double value) {
        char buffer[32];
        std::snprintf(buffer, sizeof(buffer), "%.2f", value);
        return buffer;
    }

    static std::optional<core::AnomalyEvent> check_limits(const std::string& sensor_id,
                                                          SensorRule& rule,
                                                          double value,
                                                          std::chrono::system_clock::time_point timestamp) {
        LimitState state = LimitState::inside;
        if (rule.limits.high && value > *rule.limits.high) {
            state = LimitState::above;
        } else if (rule.limits.low && value < *rule.limits.low) {
            state = LimitState::below;
        }

        const bool entered_breach = state != LimitState::inside && state != rule.limit_state;
        rule.limit_state = state;
        if (!entered_breach || !cooled_down(rule.last_limit_alert, rule.cooldown, timestamp)) {
            return std::nullopt;
        }
        rule.last_limit_alert = timestamp;

        const bool above = state == LimitState::above;
        const double limit = above ? *rule.limits.high : *rule.limits.low;

        core::AnomalyEvent event;
        event.sensor_id = sensor_id;
        event.detector_name = "limit";
        event.kind = core::AlertKind::limit;
        event.severity = core::Severity::critical;
        event.value = value;
        event.limit = limit;
        event.above_limit = above;
        event.score = std::abs(value - limit);
        event.confidence = 1.0;
        event.timestamp = timestamp;
        event.message = "value " + format_number(value) + (above ? " above high" : " below low") + " limit " +
                        format_number(limit);
        return event;
    }

    static std::optional<core::AnomalyEvent> run_detectors(const std::string& sensor_id,
                                                           SensorRule& rule,
                                                           double value,
                                                           std::chrono::system_clock::time_point timestamp) {
        if (rule.strategies.empty()) {
            return std::nullopt;
        }

        // Every strategy sees every reading (their rolling state must stay
        // continuous), even when the result is then suppressed.
        std::vector<AnomalyResult> votes;
        std::vector<std::string> voting_detector_names;
        votes.reserve(rule.strategies.size());
        for (auto& strategy : rule.strategies) {
            AnomalyResult result = strategy->observe(value);
            if (result.is_anomaly) {
                voting_detector_names.push_back(strategy->name());
            }
            votes.push_back(result);
        }

        if (voting_detector_names.size() < rule.votes_required ||
            !cooled_down(rule.last_statistical_alert, rule.cooldown, timestamp)) {
            return std::nullopt;
        }
        rule.last_statistical_alert = timestamp;

        double max_score = 0.0;
        double max_confidence = 0.0;
        for (const auto& v : votes) {
            max_score = std::max(max_score, v.score);
            max_confidence = std::max(max_confidence, v.confidence);
        }

        core::AnomalyEvent event;
        event.sensor_id = sensor_id;
        event.detector_name = join(voting_detector_names);
        event.message = "anomaly confirmed by " + std::to_string(voting_detector_names.size()) + "/" +
                        std::to_string(votes.size()) + " detector(s)";
        event.score = max_score;
        event.confidence = max_confidence;
        event.timestamp = timestamp;
        event.votes = voting_detector_names.size();
        event.detectors_total = votes.size();
        event.kind = core::AlertKind::statistical;
        event.severity = core::Severity::warning;
        event.value = value;
        return event;
    }

    static std::string join(const std::vector<std::string>& names) {
        std::string result;
        for (std::size_t i = 0; i < names.size(); ++i) {
            if (i > 0)
                result += "+";
            result += names[i];
        }
        return result;
    }

    core::EventBus& bus_;
    std::mutex mutex_;
    std::unordered_map<std::string, SensorRule> rules_;
};

}  // namespace ironpulse::analytics
