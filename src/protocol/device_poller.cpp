#include "ironpulse/protocol/device_poller.hpp"

#include "ironpulse/core/events.hpp"
#include "ironpulse/core/logger.hpp"

namespace ironpulse::protocol {

std::shared_ptr<DevicePoller> DevicePoller::create(asio::io_context& io_context,
                                                   core::ModbusDeviceConfig config,
                                                   core::EventBus& bus) {
    struct EnableMakeShared : DevicePoller {
        EnableMakeShared(asio::io_context& io, core::ModbusDeviceConfig c, core::EventBus& b)
            : DevicePoller(io, std::move(c), b) {}
    };
    return std::make_shared<EnableMakeShared>(io_context, std::move(config), bus);
}

DevicePoller::DevicePoller(asio::io_context& io_context, core::ModbusDeviceConfig config, core::EventBus& bus)
    : strand_(asio::make_strand(io_context)),
      config_(std::move(config)),
      bus_(bus),
      timer_(strand_),
      client_(ModbusClient::create(
          io_context, config_.host, config_.port, std::chrono::milliseconds(config_.timeout_ms))),
      blocks_(plan_reads(config_.sensors)) {}

void DevicePoller::start() {
    auto self = shared_from_this();
    asio::dispatch(strand_, [this, self] {
        IP_LOG_INFO("[{}] polling {}:{} unit {} every {} ms — {} sensor(s) in {} request(s)",
                    config_.id,
                    config_.host,
                    config_.port,
                    config_.unit_id,
                    config_.poll_interval_ms,
                    config_.sensors.size(),
                    blocks_.size());
        next_tick_ = std::chrono::steady_clock::now();
        run_cycle();
    });
}

void DevicePoller::stop() {
    stopped_ = true;
    auto self = shared_from_this();
    asio::dispatch(strand_, [this, self] { timer_.cancel(); });
    client_->close();
}

void DevicePoller::run_cycle() {
    if (stopped_) {
        return;
    }
    if (!client_->is_connected()) {
        auto self = shared_from_this();
        client_->connect([this, self](bool ok) {
            asio::post(strand_, [this, self, ok] {
                if (stopped_) {
                    return;
                }
                if (!ok) {
                    fail_cycle("cannot connect to " + config_.host + ":" + std::to_string(config_.port));
                    return;
                }
                read_block(0, std::make_shared<std::vector<std::optional<double>>>(config_.sensors.size()));
            });
        });
        return;
    }
    read_block(0, std::make_shared<std::vector<std::optional<double>>>(config_.sensors.size()));
}

void DevicePoller::read_block(std::size_t block_index,
                              std::shared_ptr<std::vector<std::optional<double>>> values) {
    if (block_index >= blocks_.size()) {
        finish_cycle(*values);
        return;
    }

    const ReadBlock& block = blocks_[block_index];
    auto self = shared_from_this();
    auto on_result = [this, self, block_index, values](RegistersResult result) {
        asio::post(strand_, [this, self, block_index, values, result = std::move(result)] {
            if (stopped_) {
                return;
            }
            if (const auto* error = std::get_if<ModbusError>(&result)) {
                fail_cycle(error->message);
                return;
            }
            const auto& registers = std::get<std::vector<std::uint16_t>>(result);
            const ReadBlock& done = blocks_[block_index];
            for (const std::size_t sensor_index : done.sensor_indices) {
                (*values)[sensor_index] = extract_value(done, registers, config_.sensors[sensor_index]);
            }
            read_block(block_index + 1, values);
        });
    };

    if (block.register_type == core::RegisterType::input) {
        client_->read_input_registers(
            config_.unit_id, block.start_address, block.quantity, std::move(on_result));
    } else {
        client_->read_holding_registers(
            config_.unit_id, block.start_address, block.quantity, std::move(on_result));
    }
}

void DevicePoller::finish_cycle(const std::vector<std::optional<double>>& values) {
    publish_status(true);
    const auto timestamp = std::chrono::system_clock::now();
    for (std::size_t i = 0; i < values.size(); ++i) {
        if (values[i]) {
            bus_.publish(core::SensorReadingEvent{config_.sensors[i].id, *values[i], timestamp});
        } else {
            IP_LOG_DEBUG(
                "[{}] sensor '{}' returned a non-finite value, skipped", config_.id, config_.sensors[i].id);
        }
    }
    schedule_next();
}

void DevicePoller::fail_cycle(const std::string& message) {
    bus_.publish(core::PollErrorEvent{config_.id, message});
    if (client_->is_connected()) {
        // The device answered but rejected the request (e.g. an illegal
        // address exception): it is reachable, the configuration is wrong.
        IP_LOG_WARN("[{}] poll failed: {}", config_.id, message);
        publish_status(true);
    } else {
        if (last_status_.value_or(true)) {
            IP_LOG_WARN("[{}] device unreachable: {}", config_.id, message);
        }
        publish_status(false);
    }
    schedule_next();
}

void DevicePoller::schedule_next() {
    if (stopped_) {
        return;
    }
    const auto interval = std::chrono::milliseconds(config_.poll_interval_ms);
    const auto now = std::chrono::steady_clock::now();
    next_tick_ += interval;
    if (next_tick_ < now) {
        // We fell behind (slow device, reconnect): skip the missed ticks
        // instead of firing a burst of back-to-back polls to catch up.
        next_tick_ = now + interval;
    }
    timer_.expires_at(next_tick_);
    auto self = shared_from_this();
    timer_.async_wait([this, self](const asio::error_code& ec) {
        if (!ec) {
            run_cycle();
        }
    });
}

void DevicePoller::publish_status(bool online) {
    if (last_status_ == online) {
        return;
    }
    if (online && last_status_.has_value()) {
        IP_LOG_INFO("[{}] device is back online", config_.id);
    }
    last_status_ = online;
    bus_.publish(core::DeviceStatusEvent{config_.id, online, std::chrono::system_clock::now()});
}

}  // namespace ironpulse::protocol
