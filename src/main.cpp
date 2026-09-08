#include <asio.hpp>
#include <chrono>
#include <csignal>
#include <iostream>
#include <memory>
#include <nlohmann/json.hpp>

#include "ironpulse/analytics/rule_engine.hpp"
#include "ironpulse/analytics/strategies/cusum.hpp"
#include "ironpulse/analytics/strategies/ewma.hpp"
#include "ironpulse/api/http_server.hpp"
#include "ironpulse/api/ws_server.hpp"
#include "ironpulse/core/alert_log.hpp"
#include "ironpulse/core/config.hpp"
#include "ironpulse/core/device_registry.hpp"
#include "ironpulse/core/event_bus.hpp"
#include "ironpulse/core/events.hpp"
#include "ironpulse/core/logger.hpp"
#include "ironpulse/protocol/modbus_client.hpp"
#include "ironpulse/storage/retention_policy.hpp"
#include "ironpulse/storage/series_store.hpp"
#include "ironpulse/storage/wal_writer.hpp"

namespace {

std::atomic<bool> g_running{true};

void handle_signal(int) {
    g_running = false;
}

std::string reading_to_json(const ironpulse::core::SensorReadingEvent& reading) {
    const auto ms =
        std::chrono::duration_cast<std::chrono::milliseconds>(reading.timestamp.time_since_epoch());
    nlohmann::json j{
        {"type", "reading"},
        {"sensor_id", reading.sensor_id},
        {"value", reading.value},
        {"timestamp", ms.count()},
    };
    return j.dump();
}

std::string anomaly_to_json(const ironpulse::core::AnomalyEvent& anomaly) {
    const auto ms =
        std::chrono::duration_cast<std::chrono::milliseconds>(anomaly.timestamp.time_since_epoch());
    nlohmann::json j{
        {"type", "anomaly"},
        {"sensor_id", anomaly.sensor_id},
        {"message", anomaly.message},
        {"confidence", anomaly.confidence},
        {"score", anomaly.score},
        {"timestamp", ms.count()},
    };
    return j.dump();
}

std::string device_status_to_json(const ironpulse::core::DeviceStatusEvent& status) {
    nlohmann::json j{
        {"type", "device_status"},
        {"device_id", status.device_id},
        {"online", status.online},
    };
    return j.dump();
}

/// Polls a single device on a repeating timer, publishing readings and
/// anomalies onto the shared EventBus for storage/analytics/API to
/// consume.
class DevicePoller {
public:
    DevicePoller(asio::io_context& io,
                 const ironpulse::core::ModbusDeviceConfig& cfg,
                 ironpulse::core::EventBus& bus)
        : io_(io),
          cfg_(cfg),
          bus_(bus),
          timer_(io),
          client_(ironpulse::protocol::ModbusClient::create(io, cfg.host, cfg.port)) {}

    void start() {
        attempt_connect(/*first_attempt=*/true);
    }

private:
    void attempt_connect(bool first_attempt) {
        client_->connect([this, first_attempt](bool ok) {
            publish_status(ok);
            if (!ok && first_attempt) {
                IP_LOG_WARN("[{}] initial connect failed, will retry on next tick", cfg_.id);
            }
            schedule_next();
        });
    }

    void publish_status(bool online) {
        if (online == last_known_online_) {
            return;
        }
        last_known_online_ = online;
        bus_.publish(ironpulse::core::DeviceStatusEvent{cfg_.id, online, std::chrono::system_clock::now()});
    }

    void schedule_next() {
        if (!g_running)
            return;
        timer_.expires_after(std::chrono::milliseconds(cfg_.poll_interval_ms));
        timer_.async_wait([this](const asio::error_code& ec) {
            if (ec || !g_running)
                return;
            poll_once();
        });
    }

    void poll_once() {
        if (!client_->is_connected()) {
            attempt_connect(/*first_attempt=*/false);
            return;
        }
        poll_registers();
    }

    void poll_registers() {
        client_->read_holding_registers(
            cfg_.unit_id,
            /*start_address=*/0,
            /*quantity=*/1,
            [this](ironpulse::protocol::RegistersResult result) {
                if (auto* regs = std::get_if<std::vector<std::uint16_t>>(&result)) {
                    if (!regs->empty()) {
                        const double value = static_cast<double>((*regs)[0]);
                        const auto timestamp = std::chrono::system_clock::now();

                        bus_.publish(ironpulse::core::SensorReadingEvent{cfg_.id, value, timestamp});
                    }
                } else {
                    auto& err = std::get<ironpulse::protocol::ModbusError>(result);
                    IP_LOG_ERROR("[{}] read failed: {}", cfg_.id, err.message);
                    publish_status(false);
                }
                schedule_next();
            });
    }

    asio::io_context& io_;
    ironpulse::core::ModbusDeviceConfig cfg_;
    ironpulse::core::EventBus& bus_;
    asio::steady_timer timer_;
    std::shared_ptr<ironpulse::protocol::ModbusClient> client_;
    bool last_known_online_ = false;
};

}  // namespace

int main(int argc, char** argv) {
    std::signal(SIGINT, handle_signal);
    std::signal(SIGTERM, handle_signal);

    const std::string config_path = argc > 1 ? argv[1] : "config.json";

    ironpulse::core::AppConfig config;
    try {
        config = ironpulse::core::AppConfig::load_from_file(config_path);
    } catch (const std::exception& e) {
        std::cerr << "Failed to load config '" << config_path << "': " << e.what() << "\n";
        return 1;
    }

    ironpulse::core::Logger::init(config.app_name, ironpulse::core::LogLevel::info, config.log_file);
    IP_LOG_INFO("ironpulse starting — {} device(s) configured", config.devices.size());

    // -------------------------------------------------------------------
    // Wiring: EventBus is the spine connecting protocol -> storage,
    // analytics, and the API layer without those layers depending on
    // each other directly.
    // -------------------------------------------------------------------
    ironpulse::core::EventBus bus;
    ironpulse::core::AlertLog alert_log(200);
    ironpulse::core::DeviceRegistry device_registry;

    std::shared_ptr<ironpulse::storage::WalWriter> wal;
    if (config.persistence_enabled) {
        wal = std::make_shared<ironpulse::storage::WalWriter>(config.data_dir);
        IP_LOG_INFO("Persistence enabled, WAL directory: {}", config.data_dir);
    }
    ironpulse::storage::SeriesStore store(
        /*buffer_capacity_per_series=*/4096, wal, std::chrono::hours(config.retention_hours));

    ironpulse::analytics::RuleEngine rule_engine(bus);
    for (const auto& device_cfg : config.devices) {
        std::vector<std::unique_ptr<ironpulse::analytics::AnomalyDetector>> strategies;
        strategies.push_back(std::make_unique<ironpulse::analytics::ZScoreDetector>(60, 3.0));
        strategies.push_back(std::make_unique<ironpulse::analytics::EwmaDetector>(0.2, 3.0));
        // Require at least one strategy to agree — with only two fast
        // (window=60) detectors on a low-rate demo feed, requiring both
        // would rarely fire in a short demo session. Real deployments
        // with more strategies (see CUSUM below, once given a
        // domain-appropriate reference mean) should raise this.
        rule_engine.register_sensor(device_cfg.id, std::move(strategies), /*votes_required=*/1);
    }

    // Subscriptions: fan out each event to persistence-adjacent state and
    // to both live API surfaces (REST reads current state; WS pushes the
    // event itself).
    ironpulse::api::WsServer* ws_server_ptr = nullptr;  // set once constructed below

    bus.subscribe<ironpulse::core::SensorReadingEvent>(
        [&store, &rule_engine, &ws_server_ptr](const ironpulse::core::SensorReadingEvent& reading) {
            store.record(reading.sensor_id, reading.value, reading.timestamp);
            rule_engine.observe(reading.sensor_id, reading.value, reading.timestamp);
            if (ws_server_ptr) {
                ws_server_ptr->broadcast(reading_to_json(reading));
            }
        });

    bus.subscribe<ironpulse::core::AnomalyEvent>(
        [&alert_log, &ws_server_ptr](const ironpulse::core::AnomalyEvent& anomaly) {
            IP_LOG_WARN("[{}] ANOMALY {} score={:.2f} confidence={:.0f}%",
                        anomaly.sensor_id,
                        anomaly.message,
                        anomaly.score,
                        anomaly.confidence * 100);
            alert_log.push(anomaly);
            if (ws_server_ptr) {
                ws_server_ptr->broadcast(anomaly_to_json(anomaly));
            }
        });

    bus.subscribe<ironpulse::core::DeviceStatusEvent>(
        [&device_registry, &ws_server_ptr](const ironpulse::core::DeviceStatusEvent& status) {
            IP_LOG_INFO("[{}] device is now {}", status.device_id, status.online ? "online" : "offline");
            device_registry.set_status(status.device_id, status.online);
            if (ws_server_ptr) {
                ws_server_ptr->broadcast(device_status_to_json(status));
            }
        });

    // -------------------------------------------------------------------
    // Networking: one io_context shared by Modbus polling and the
    // WebSocket server; the REST server runs its own blocking accept loop
    // on a dedicated thread (cpp-httplib's model).
    // -------------------------------------------------------------------
    asio::io_context io_context;

    std::vector<std::unique_ptr<DevicePoller>> pollers;
    for (const auto& device_cfg : config.devices) {
        auto poller = std::make_unique<DevicePoller>(io_context, device_cfg, bus);
        poller->start();
        pollers.push_back(std::move(poller));
    }

    ironpulse::api::WsServer ws_server(io_context, config.ws_port);
    ws_server.start();
    ws_server_ptr = &ws_server;

    ironpulse::api::HttpServer http_server(
        store, alert_log, device_registry, config.http_port, config.ws_port, config.web_root);
    http_server.start();

    // Periodic WAL flush + retention pruning, driven off the same
    // io_context via a repeating timer (no extra thread needed).
    asio::steady_timer maintenance_timer(io_context);
    std::function<void()> schedule_maintenance = [&]() {
        if (!g_running)
            return;
        maintenance_timer.expires_after(std::chrono::seconds(5));
        maintenance_timer.async_wait([&](const asio::error_code& ec) {
            if (ec || !g_running)
                return;
            if (wal) {
                wal->flush_all();
            }
            schedule_maintenance();
        });
    };
    if (wal) {
        schedule_maintenance();
    }

    std::vector<std::thread> io_threads;
    const std::size_t thread_count = std::max<std::size_t>(1, config.worker_threads);
    for (std::size_t i = 0; i < thread_count; ++i) {
        io_threads.emplace_back([&io_context] { io_context.run(); });
    }

    while (g_running) {
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }

    IP_LOG_INFO("Shutting down...");
    http_server.stop();
    if (wal) {
        wal->flush_all();
    }
    io_context.stop();
    for (auto& t : io_threads) {
        if (t.joinable())
            t.join();
    }
    return 0;
}
