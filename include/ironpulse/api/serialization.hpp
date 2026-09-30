#pragma once

#include <chrono>
#include <nlohmann/json.hpp>

#include "ironpulse/core/events.hpp"

namespace ironpulse::api {

/// JSON fields shared by the REST alert list and the WebSocket "anomaly"
/// event, so both surfaces describe an alert identically. The timestamp
/// is added by each caller (ISO 8601 for REST, epoch millis for WS).
[[nodiscard]] inline nlohmann::json alert_fields(const core::AnomalyEvent& event) {
    nlohmann::json j{
        {"sensor_id", event.sensor_id},
        {"kind", core::to_string(event.kind)},
        {"severity", core::to_string(event.severity)},
        {"detector", event.detector_name},
        {"message", event.message},
        {"value", event.value},
        {"votes", event.votes},
        {"detectors_total", event.detectors_total},
        {"score", event.score},
        {"confidence", event.confidence},
    };
    if (event.limit) {
        j["limit"] = *event.limit;
        j["direction"] = event.above_limit ? "high" : "low";
    }
    return j;
}

[[nodiscard]] inline std::int64_t to_epoch_ms(std::chrono::system_clock::time_point tp) {
    return std::chrono::duration_cast<std::chrono::milliseconds>(tp.time_since_epoch()).count();
}

}  // namespace ironpulse::api
