#pragma once

#include <asio.hpp>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "ironpulse/core/config.hpp"
#include "ironpulse/core/event_bus.hpp"
#include "ironpulse/protocol/modbus_client.hpp"
#include "ironpulse/protocol/register_decoder.hpp"

namespace ironpulse::protocol {

/// Polls one Modbus device on a fixed schedule and publishes its sensors'
/// readings onto the EventBus.
///
/// Each cycle reads every planned register block (see plan_reads) one
/// after another — Modbus TCP allows a single outstanding request per
/// connection — and publishes all of the device's readings with one
/// shared timestamp, so values read together stay aligned in storage and
/// charts. Cycles run at a fixed rate rather than "interval after the
/// previous cycle finished", so a slow device does not make the sampling
/// period drift.
///
/// Publishes:
///   SensorReadingEvent  per sensor per successful cycle
///   DeviceStatusEvent   on startup and on every online/offline transition
///   PollErrorEvent      for every failed request
class DevicePoller : public std::enable_shared_from_this<DevicePoller> {
public:
    static std::shared_ptr<DevicePoller> create(asio::io_context& io_context,
                                                core::ModbusDeviceConfig config,
                                                core::EventBus& bus);

    void start();
    void stop();

    [[nodiscard]] const core::ModbusDeviceConfig& config() const noexcept {
        return config_;
    }

private:
    DevicePoller(asio::io_context& io_context, core::ModbusDeviceConfig config, core::EventBus& bus);

    void run_cycle();
    void read_block(std::size_t block_index, std::shared_ptr<std::vector<std::optional<double>>> values);
    void finish_cycle(const std::vector<std::optional<double>>& values);
    void fail_cycle(const std::string& message);
    void schedule_next();
    void publish_status(bool online);

    asio::strand<asio::io_context::executor_type> strand_;
    core::ModbusDeviceConfig config_;
    core::EventBus& bus_;
    asio::steady_timer timer_;
    std::shared_ptr<ModbusClient> client_;
    std::vector<ReadBlock> blocks_;
    std::chrono::steady_clock::time_point next_tick_;
    std::optional<bool> last_status_;
    std::atomic<bool> stopped_{false};
};

}  // namespace ironpulse::protocol
