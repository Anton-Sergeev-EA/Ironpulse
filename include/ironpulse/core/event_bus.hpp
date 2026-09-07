#pragma once

#include <algorithm>
#include <functional>
#include <mutex>
#include <typeindex>
#include <unordered_map>
#include <vector>

namespace ironpulse::core {

/// A minimal, thread-safe, type-erased publish/subscribe event bus.
///
/// Layers (protocol, storage, analytics, api) communicate through events
/// instead of direct calls, which keeps them independently testable.
///
/// Usage:
///   struct SensorReading { std::string sensor_id; double value; };
///
///   EventBus bus;
///   auto token = bus.subscribe<SensorReading>([](const SensorReading& r) {
///       // handle reading
///   });
///   bus.publish(SensorReading{"temp_01", 42.5});
///   bus.unsubscribe<SensorReading>(token);
class EventBus {
public:
    using SubscriptionId = std::size_t;

    template <typename Event>
    SubscriptionId subscribe(std::function<void(const Event&)> handler) {
        std::lock_guard lock(mutex_);
        auto& slot = handlers_[std::type_index(typeid(Event))];
        const SubscriptionId id = next_id_++;
        slot.emplace_back(id, [handler = std::move(handler)](const void* event) {
            handler(*static_cast<const Event*>(event));
        });
        return id;
    }

    template <typename Event>
    void unsubscribe(SubscriptionId id) {
        std::lock_guard lock(mutex_);
        auto it = handlers_.find(std::type_index(typeid(Event)));
        if (it == handlers_.end()) {
            return;
        }
        auto& slot = it->second;
        slot.erase(
            std::remove_if(slot.begin(), slot.end(), [id](const auto& entry) { return entry.first == id; }),
            slot.end());
    }

    template <typename Event>
    void publish(const Event& event) const {
        std::vector<std::function<void(const void*)>> callbacks;
        {
            std::lock_guard lock(mutex_);
            auto it = handlers_.find(std::type_index(typeid(Event)));
            if (it == handlers_.end()) {
                return;
            }
            callbacks.reserve(it->second.size());
            for (const auto& [id, fn] : it->second) {
                callbacks.push_back(fn);
            }
        }
        for (const auto& cb : callbacks) {
            cb(&event);
        }
    }

private:
    using Entry = std::pair<SubscriptionId, std::function<void(const void*)>>;

    mutable std::mutex mutex_;
    std::unordered_map<std::type_index, std::vector<Entry>> handlers_;
    SubscriptionId next_id_ = 1;
};

}  // namespace ironpulse::core
