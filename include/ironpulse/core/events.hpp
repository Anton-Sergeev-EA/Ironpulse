#pragma once

#include <chrono>
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>

namespace ironpulse::core {

/// A single sensor reading, published by the protocol layer after every
/// successful poll and consumed by storage, analytics, and the API layer.
/// `value` is already in engineering units (scale/offset applied).
struct SensorReadingEvent {
    std::string sensor_id;
    double value;
    std::chrono::system_clock::time_point timestamp;
};

/// How urgent an alert is. Statistical anomalies are early warnings that a
/// sensor is behaving unusually; crossing a configured hard limit is
/// critical by definition.
enum class Severity { warning, critical };

/// What raised the alert.
enum class AlertKind {
    statistical,  // a quorum of anomaly detectors agreed (RuleEngine)
    limit,        // the value crossed a configured low/high limit
};

[[nodiscard]] constexpr std::string_view to_string(Severity severity) noexcept {
    return severity == Severity::critical ? "critical" : "warning";
}

[[nodiscard]] constexpr std::string_view to_string(AlertKind kind) noexcept {
    return kind == AlertKind::limit ? "limit" : "statistical";
}

/// Raised by the analytics layer when a detector quorum flags a reading
/// as anomalous, or when a reading crosses a configured limit.
struct AnomalyEvent {
    std::string sensor_id;
    std::string detector_name;
    std::string message;
    double score = 0.0;
    double confidence = 0.0;
    std::chrono::system_clock::time_point timestamp;
    /// How many strategies voted "anomaly" out of how many are registered
    /// for this sensor. Lets clients render `message` in their own
    /// language instead of relying on the engine's English text.
    std::size_t votes = 0;
    std::size_t detectors_total = 0;

    AlertKind kind = AlertKind::statistical;
    Severity severity = Severity::warning;
    /// The reading that triggered the alert, in engineering units.
    double value = 0.0;
    /// For limit alerts: the limit that was crossed, and on which side.
    std::optional<double> limit;
    bool above_limit = false;
};

/// Raised by the protocol layer when a poll of a device fails (timeout,
/// connection loss, Modbus exception). Counted by the metrics endpoint.
struct PollErrorEvent {
    std::string device_id;
    std::string message;
};

/// Raised by the protocol layer whenever a device transitions between
/// connected/disconnected.
struct DeviceStatusEvent {
    std::string device_id;
    bool online;
    std::chrono::system_clock::time_point timestamp;
};

}  // namespace ironpulse::core
