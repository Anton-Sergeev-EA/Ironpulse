#pragma once

#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include "ironpulse/analytics/detector.hpp"
#include "ironpulse/core/event_bus.hpp"
#include "ironpulse/core/events.hpp"

namespace ironpulse::analytics {

/// Combines multiple AnomalyDetector strategies per sensor and only raises
/// an alert when at least `votes_required` of them agree on the same
/// reading. This is what turns "a detector fired" into "an alert worth
/// waking someone up for" — a lone z-score blip on noisy sensor data is
/// common; z-score *and* CUSUM *and* EWMA agreeing is not.
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
        std::lock_guard lock(mutex_);
        SensorRule rule;
        rule.strategies = std::move(strategies);
        rule.votes_required = votes_required == 0 ? rule.strategies.size() : votes_required;
        rules_[sensor_id] = std::move(rule);
    }

    /// Feeds a new reading through all registered strategies for this
    /// sensor. Publishes a core::AnomalyEvent on the EventBus if the vote
    /// quorum is reached. No-op (but harmless) for unregistered sensors.
    void observe(const std::string& sensor_id,
                 double value,
                 std::chrono::system_clock::time_point timestamp) {
        std::vector<AnomalyResult> votes;
        std::vector<std::string> voting_detector_names;
        std::size_t votes_required = 0;

        {
            std::lock_guard lock(mutex_);
            auto it = rules_.find(sensor_id);
            if (it == rules_.end()) {
                return;
            }
            votes_required = it->second.votes_required;
            votes.reserve(it->second.strategies.size());
            for (auto& strategy : it->second.strategies) {
                AnomalyResult result = strategy->observe(value);
                if (result.is_anomaly) {
                    voting_detector_names.push_back(strategy->name());
                }
                votes.push_back(result);
            }
        }

        if (voting_detector_names.size() < votes_required) {
            return;
        }

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

        bus_.publish(event);
    }

private:
    struct SensorRule {
        std::vector<std::unique_ptr<AnomalyDetector>> strategies;
        std::size_t votes_required = 1;
    };

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
