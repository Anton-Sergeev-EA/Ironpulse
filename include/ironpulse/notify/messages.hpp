#pragma once

#include <chrono>
#include <map>
#include <optional>
#include <string>

#include "ironpulse/core/config.hpp"
#include "ironpulse/core/events.hpp"

namespace ironpulse::notify {

/// Sensor metadata a notification needs to be readable by a human
/// ("Winding temperature 142 °C" rather than "transformer_temp_01: 142").
struct SensorInfo {
    std::string name;
    std::string unit;
    std::string device_id;
};

/// Everything a channel can say about one event, in a channel-neutral form.
struct Notification {
    enum class Type { alert, device_offline, device_online };

    Type type = Type::alert;
    core::Severity severity = core::Severity::warning;
    std::string title;  // one line, localized
    std::string text;   // full message incl. title, localized, plain text
    std::chrono::system_clock::time_point timestamp;
    std::string sensor_id;  // empty for device notifications
    std::string device_id;
    std::optional<core::AnomalyEvent> alert;
};

/// Builds the localized notification for an alert. `language` is one of
/// the supported interface languages; unknown codes fall back to Russian.
[[nodiscard]] Notification describe_alert(const core::AnomalyEvent& alert,
                                          const SensorInfo& sensor,
                                          const std::string& language,
                                          const std::string& dashboard_url);

/// Builds the localized notification for a device going offline/online.
[[nodiscard]] Notification describe_device_status(const std::string& device_id,
                                                  bool online,
                                                  std::chrono::system_clock::time_point timestamp,
                                                  const std::string& language,
                                                  const std::string& dashboard_url);

/// Formats a number for humans: at most two decimals, no trailing zeros.
[[nodiscard]] std::string format_value(double value);

}  // namespace ironpulse::notify
