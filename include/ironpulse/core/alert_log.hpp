#pragma once

#include <deque>
#include <mutex>
#include <vector>

#include "ironpulse/core/events.hpp"

namespace ironpulse::core {

/// Bounded history of recent AnomalyEvents, so the REST API can answer
/// "GET /api/v1/alerts" without needing a full database — the dashboard
/// only ever needs the last N alerts to render its panel.
class AlertLog {
public:
    explicit AlertLog(std::size_t capacity = 200) : capacity_(capacity) {}

    void push(const AnomalyEvent& event) {
        std::lock_guard lock(mutex_);
        events_.push_back(event);
        while (events_.size() > capacity_) {
            events_.pop_front();
        }
    }

    /// Returns up to `count` most recent alerts, newest first.
    [[nodiscard]] std::vector<AnomalyEvent> recent(std::size_t count) const {
        std::lock_guard lock(mutex_);
        count = std::min(count, events_.size());
        std::vector<AnomalyEvent> result;
        result.reserve(count);
        for (auto it = events_.rbegin(); it != events_.rbegin() + static_cast<std::ptrdiff_t>(count); ++it) {
            result.push_back(*it);
        }
        return result;
    }

private:
    std::size_t capacity_;
    mutable std::mutex mutex_;
    std::deque<AnomalyEvent> events_;
};

}  // namespace ironpulse::core
