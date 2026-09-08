#pragma once

#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace ironpulse::core {

/// Tracks the current online/offline status of every known device, for
/// "GET /api/v1/devices" and the dashboard's device list. Updated by
/// subscribing to DeviceStatusEvent on the EventBus.
class DeviceRegistry {
public:
    void set_status(const std::string& device_id, bool online) {
        std::lock_guard lock(mutex_);
        status_[device_id] = online;
    }

    struct DeviceStatus {
        std::string device_id;
        bool online;
    };

    [[nodiscard]] std::vector<DeviceStatus> all() const {
        std::lock_guard lock(mutex_);
        std::vector<DeviceStatus> result;
        result.reserve(status_.size());
        for (const auto& [id, online] : status_) {
            result.push_back({id, online});
        }
        return result;
    }

private:
    mutable std::mutex mutex_;
    std::unordered_map<std::string, bool> status_;
};

}  // namespace ironpulse::core
