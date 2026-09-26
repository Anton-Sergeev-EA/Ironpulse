#pragma once

#include <chrono>
#include <cstddef>
#include <string>

namespace ironpulse::core {

/// A single sensor reading, published by the protocol layer after every
/// successful poll and consumed by storage, analytics, and the API layer.
struct SensorReadingEvent {
    std::string sensor_id;
    double value;
    std::chrono::system_clock::time_point timestamp;
};

/// Raised by the analytics layer when a detector (or a quorum of
/// detectors, via RuleEngine) flags a reading as anomalous.
struct AnomalyEvent {
    std::string sensor_id;
    std::string detector_name;
    std::string message;
    double score;
    double confidence;
    std::chrono::system_clock::time_point timestamp;
    /// How many strategies voted "anomaly" out of how many are registered
    /// for this sensor. Lets clients render `message` in their own
    /// language instead of relying on the engine's English text.
    std::size_t votes = 0;
    std::size_t detectors_total = 0;
};

/// Raised by the protocol layer whenever a device transitions between
/// connected/disconnected.
struct DeviceStatusEvent {
    std::string device_id;
    bool online;
    std::chrono::system_clock::time_point timestamp;
};

}  // namespace ironpulse::core
