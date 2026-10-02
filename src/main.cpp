#include <asio.hpp>
#include <atomic>
#include <chrono>
#include <csignal>
#include <iostream>
#include <map>
#include <memory>
#include <nlohmann/json.hpp>
#include <string>
#include <thread>
#include <vector>

#include "ironpulse/analytics/detector_factory.hpp"
#include "ironpulse/analytics/rule_engine.hpp"
#include "ironpulse/api/http_server.hpp"
#include "ironpulse/api/serialization.hpp"
#include "ironpulse/api/ws_server.hpp"
#include "ironpulse/core/alert_log.hpp"
#include "ironpulse/core/config.hpp"
#include "ironpulse/core/device_registry.hpp"
#include "ironpulse/core/event_bus.hpp"
#include "ironpulse/core/events.hpp"
#include "ironpulse/core/logger.hpp"
#include "ironpulse/core/metrics.hpp"
#include "ironpulse/ingest/reading_exporter.hpp"
#include "ironpulse/notify/notifier.hpp"
#include "ironpulse/protocol/device_poller.hpp"
#include "ironpulse/storage/series_store.hpp"
#include "ironpulse/storage/wal_writer.hpp"
#include "ironpulse/version.hpp"

namespace {

std::atomic<bool> g_running{true};

void handle_signal(int) {
    g_running = false;
}

std::string reading_to_json(const ironpulse::core::SensorReadingEvent& reading) {
    nlohmann::json j{
        {"type", "reading"},
        {"sensor_id", reading.sensor_id},
        {"value", reading.value},
        {"timestamp", ironpulse::api::to_epoch_ms(reading.timestamp)},
    };
    return j.dump();
}

std::string anomaly_to_json(const ironpulse::core::AnomalyEvent& anomaly) {
    nlohmann::json j = ironpulse::api::alert_fields(anomaly);
    j["type"] = "anomaly";
    j["timestamp"] = ironpulse::api::to_epoch_ms(anomaly.timestamp);
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

ironpulse::core::LogLevel parse_log_level(const std::string& level) {
    using ironpulse::core::LogLevel;
    if (level == "trace")
        return LogLevel::trace;
    if (level == "debug")
        return LogLevel::debug;
    if (level == "warn")
        return LogLevel::warn;
    if (level == "error")
        return LogLevel::error;
    if (level == "critical")
        return LogLevel::critical;
    return LogLevel::info;
}

void describe_metrics(ironpulse::core::Metrics& metrics) {
    using Type = ironpulse::core::Metrics::Type;
    metrics.describe("ironpulse_build_info", Type::gauge, "Build information; the value is always 1.");
    metrics.describe("ironpulse_uptime_seconds", Type::gauge, "Seconds since the engine started.");
    metrics.describe("ironpulse_readings_total", Type::counter, "Sensor readings received.");
    metrics.describe(
        "ironpulse_sensor_value", Type::gauge, "Latest value of each sensor, in engineering units.");
    metrics.describe("ironpulse_alerts_total", Type::counter, "Alerts raised, by sensor, kind and severity.");
    metrics.describe("ironpulse_device_up", Type::gauge, "1 if the device answered its last poll, else 0.");
    metrics.describe("ironpulse_poll_errors_total", Type::counter, "Failed Modbus requests per device.");
    metrics.describe("ironpulse_websocket_clients", Type::gauge, "Connected dashboard WebSocket clients.");
    metrics.describe(
        "ironpulse_notifications_total", Type::counter, "Notification deliveries by channel and result.");
    metrics.describe("ironpulse_export_readings_total", Type::counter, "Readings queued for export.");
    metrics.describe(
        "ironpulse_export_dropped_total", Type::counter, "Readings not exported because the queue was full.");
    metrics.describe("ironpulse_export_batches_total", Type::counter, "Export batches written to disk.");
    metrics.describe(
        "ironpulse_export_batch_errors_total", Type::counter, "Export batches lost to an I/O error.");
    metrics.describe("ironpulse_export_bytes_total", Type::counter, "Bytes written to export segments.");
    metrics.describe("ironpulse_export_segments_total", Type::counter, "Export segment files completed.");
    metrics.describe("ironpulse_export_queue_depth", Type::gauge, "Readings waiting to be exported.");
}

/// Copies the exporter's counters into the Prometheus registry and warns
/// once per housekeeping tick if readings were dropped since the last one.
void publish_export_stats(const ironpulse::ingest::ReadingExporter& exporter,
                          ironpulse::core::Metrics& metrics,
                          std::uint64_t& last_dropped) {
    const auto stats = exporter.stats();
    metrics.set("ironpulse_export_readings_total", static_cast<double>(stats.accepted));
    metrics.set("ironpulse_export_dropped_total", static_cast<double>(stats.dropped));
    metrics.set("ironpulse_export_batches_total", static_cast<double>(stats.batches_written));
    metrics.set("ironpulse_export_batch_errors_total", static_cast<double>(stats.batches_failed));
    metrics.set("ironpulse_export_bytes_total", static_cast<double>(stats.bytes_written));
    metrics.set("ironpulse_export_segments_total", static_cast<double>(stats.segments_completed));
    metrics.set("ironpulse_export_queue_depth", static_cast<double>(stats.queue_depth));
    if (stats.dropped > last_dropped) {
        IP_LOG_WARN("Export: {} reading(s) dropped — the disk is not keeping up; "
                    "raise export.queue_capacity or check the export directory",
                    stats.dropped - last_dropped);
        last_dropped = stats.dropped;
    }
}

}  // namespace

int main(int argc, char** argv) {
    std::signal(SIGINT, handle_signal);
    std::signal(SIGTERM, handle_signal);

    if (argc > 1 && (std::string(argv[1]) == "--version" || std::string(argv[1]) == "-v")) {
        std::cout << "ironpulse " << ironpulse::kVersion << "\n";
        return 0;
    }
    const std::string config_path = argc > 1 ? argv[1] : "config.json";

    ironpulse::core::AppConfig config;
    try {
        config = ironpulse::core::AppConfig::load_from_file(config_path);
    } catch (const std::exception& e) {
        std::cerr << "Failed to load config '" << config_path << "': " << e.what() << "\n";
        return 1;
    }

    ironpulse::core::Logger::init(config.app_name, parse_log_level(config.log_level), config.log_file);
    std::size_t sensor_count = 0;
    for (const auto& device : config.devices) {
        sensor_count += device.sensors.size();
    }
    IP_LOG_INFO("ironpulse {} starting — {} device(s), {} sensor(s)",
                ironpulse::kVersion,
                config.devices.size(),
                sensor_count);
    for (const auto& warning : config.warnings) {
        IP_LOG_WARN("config: {}", warning);
    }
    if (config.api_token.empty()) {
        IP_LOG_WARN(
            "No api_token set — the API and dashboard data are readable by anyone who can reach this port");
    }

    const auto started_at = std::chrono::steady_clock::now();
    ironpulse::core::Metrics metrics;
    describe_metrics(metrics);
    metrics.set("ironpulse_build_info", {{"version", ironpulse::kVersion}}, 1.0);

    // -------------------------------------------------------------------
    // Wiring: EventBus is the spine connecting protocol -> storage,
    // analytics, notifications and the API layer without those layers
    // depending on each other directly.
    // -------------------------------------------------------------------
    ironpulse::core::EventBus bus;
    ironpulse::core::AlertLog alert_log(500);
    ironpulse::core::DeviceRegistry device_registry;

    std::shared_ptr<ironpulse::storage::WalWriter> wal;
    if (config.persistence_enabled) {
        wal = std::make_shared<ironpulse::storage::WalWriter>(config.data_dir);
        IP_LOG_INFO("Persistence enabled, WAL directory: {}", config.data_dir);
    }
    const auto retention = std::chrono::hours(config.retention_hours);
    ironpulse::storage::SeriesStore store(/*buffer_capacity_per_series=*/4096, wal, retention);

    std::unique_ptr<ironpulse::ingest::ReadingExporter> exporter;
    if (config.export_config.enabled) {
        try {
            exporter = std::make_unique<ironpulse::ingest::ReadingExporter>(
                config.export_config, ironpulse::ingest::tags_from_devices(config.devices));
        } catch (const std::exception& e) {
            IP_LOG_CRITICAL("Cannot start export to '{}': {}", config.export_config.directory, e.what());
            return 1;
        }
        if (exporter->recovered_segments() > 0) {
            IP_LOG_WARN("Export: finalised {} segment(s) left unfinished by a previous crash",
                        exporter->recovered_segments());
        }
        exporter->start();
        IP_LOG_INFO("Export enabled: {} (batch {} readings / {} ms, segments up to {} MB)",
                    config.export_config.directory,
                    config.export_config.batch_size,
                    config.export_config.flush_interval_ms,
                    config.export_config.segment_max_mb);
    }

    ironpulse::analytics::RuleEngine rule_engine(bus);
    std::map<std::string, ironpulse::notify::SensorInfo> sensor_info;
    std::map<std::string, std::string> sensor_unit;
    for (const auto& device : config.devices) {
        for (const auto& sensor : device.sensors) {
            ironpulse::analytics::register_sensor(rule_engine, sensor);
            sensor_info[sensor.id] = {sensor.name, sensor.unit, device.id};
            sensor_unit[sensor.id] = sensor.unit;
        }
    }

    ironpulse::notify::Notifier notifier(
        config.notifications, sensor_info, [&metrics](const std::string& channel, bool ok) {
            metrics.increment("ironpulse_notifications_total",
                              {{"channel", channel}, {"result", ok ? "ok" : "error"}});
        });

    // Set once the WebSocket server exists (below); events published
    // before that simply aren't broadcast.
    std::atomic<ironpulse::api::WsServer*> ws_server_ptr{nullptr};

    bus.subscribe<ironpulse::core::SensorReadingEvent>(
        [&](const ironpulse::core::SensorReadingEvent& reading) {
            store.record(reading.sensor_id, reading.value, reading.timestamp);
            if (exporter) {
                exporter->submit(reading.sensor_id, reading.value, reading.timestamp);
            }
            metrics.increment("ironpulse_readings_total", {{"sensor", reading.sensor_id}});
            const auto unit = sensor_unit.find(reading.sensor_id);  // read-only: safe across threads
            metrics.set(
                "ironpulse_sensor_value",
                {{"sensor", reading.sensor_id}, {"unit", unit != sensor_unit.end() ? unit->second : ""}},
                reading.value);
            rule_engine.observe(reading.sensor_id, reading.value, reading.timestamp);
            if (auto* ws = ws_server_ptr.load()) {
                ws->broadcast(reading_to_json(reading));
            }
        });

    bus.subscribe<ironpulse::core::AnomalyEvent>([&](const ironpulse::core::AnomalyEvent& anomaly) {
        IP_LOG_WARN("[{}] {} {}: {} (score={:.2f}, confidence={:.0f}%)",
                    anomaly.sensor_id,
                    ironpulse::core::to_string(anomaly.severity),
                    ironpulse::core::to_string(anomaly.kind),
                    anomaly.message,
                    anomaly.score,
                    anomaly.confidence * 100);
        alert_log.push(anomaly);
        metrics.increment("ironpulse_alerts_total",
                          {{"sensor", anomaly.sensor_id},
                           {"kind", std::string(ironpulse::core::to_string(anomaly.kind))},
                           {"severity", std::string(ironpulse::core::to_string(anomaly.severity))}});
        notifier.on_alert(anomaly);
        if (auto* ws = ws_server_ptr.load()) {
            ws->broadcast(anomaly_to_json(anomaly));
        }
    });

    bus.subscribe<ironpulse::core::DeviceStatusEvent>([&](const ironpulse::core::DeviceStatusEvent& status) {
        IP_LOG_INFO("[{}] device is now {}", status.device_id, status.online ? "online" : "offline");
        device_registry.set_status(status.device_id, status.online);
        metrics.set("ironpulse_device_up", {{"device", status.device_id}}, status.online ? 1.0 : 0.0);
        notifier.on_device_status(status.device_id, status.online, status.timestamp);
        if (auto* ws = ws_server_ptr.load()) {
            ws->broadcast(device_status_to_json(status));
        }
    });

    bus.subscribe<ironpulse::core::PollErrorEvent>([&](const ironpulse::core::PollErrorEvent& error) {
        metrics.increment("ironpulse_poll_errors_total", {{"device", error.device_id}});
    });

    // -------------------------------------------------------------------
    // Networking: one io_context shared by Modbus polling and the
    // WebSocket server; the REST server runs its own accept loop on a
    // dedicated thread (cpp-httplib's model).
    // -------------------------------------------------------------------
    asio::io_context io_context;
    auto work_guard = asio::make_work_guard(io_context);

    std::unique_ptr<ironpulse::api::WsServer> ws_server;
    try {
        ws_server = std::make_unique<ironpulse::api::WsServer>(io_context, config.ws_port, config.api_token);
    } catch (const std::exception& e) {
        IP_LOG_CRITICAL("Cannot open WebSocket port {}: {}", config.ws_port, e.what());
        return 1;
    }
    ws_server->start();
    ws_server_ptr = ws_server.get();

    ironpulse::api::HttpServerOptions http_options;
    http_options.port = config.http_port;
    http_options.ws_port = config.ws_port;
    http_options.web_root = config.web_root;
    http_options.api_token = config.api_token;
    ironpulse::api::HttpServer http_server(
        store, alert_log, device_registry, config.devices, metrics, http_options);
    if (!http_server.start()) {
        IP_LOG_CRITICAL("Cannot open HTTP port {} — is another process using it?", config.http_port);
        return 1;
    }

    notifier.start();

    std::vector<std::shared_ptr<ironpulse::protocol::DevicePoller>> pollers;
    for (const auto& device_cfg : config.devices) {
        auto poller = ironpulse::protocol::DevicePoller::create(io_context, device_cfg, bus);
        poller->start();
        pollers.push_back(std::move(poller));
    }

    // Housekeeping on the shared io_context: WAL flush and gauges every
    // 5 s, retention pruning every hour (so the WAL never outgrows
    // retention_hours).
    asio::steady_timer maintenance_timer(io_context);
    auto last_prune = std::chrono::steady_clock::now();
    std::uint64_t last_export_dropped = 0;
    std::function<void()> schedule_maintenance = [&]() {
        maintenance_timer.expires_after(std::chrono::seconds(5));
        maintenance_timer.async_wait([&](const asio::error_code& ec) {
            if (ec || !g_running) {
                return;
            }
            const auto now = std::chrono::steady_clock::now();
            metrics.set("ironpulse_uptime_seconds", std::chrono::duration<double>(now - started_at).count());
            metrics.set("ironpulse_websocket_clients", static_cast<double>(ws_server->connection_count()));
            if (exporter) {
                publish_export_stats(*exporter, metrics, last_export_dropped);
            }
            if (wal) {
                wal->flush_all();
                if (now - last_prune >= std::chrono::hours(1)) {
                    last_prune = now;
                    const auto removed = wal->prune_older_than(retention);
                    if (removed > 0) {
                        IP_LOG_INFO("Retention: removed {} record(s) older than {} h",
                                    removed,
                                    config.retention_hours);
                    }
                }
            }
            schedule_maintenance();
        });
    };
    if (wal) {
        // Apply retention once at startup too, in case the engine was down
        // for longer than the retention period.
        wal->prune_older_than(retention);
    }
    schedule_maintenance();

    std::vector<std::thread> io_threads;
    for (std::size_t i = 0; i < config.worker_threads; ++i) {
        io_threads.emplace_back([&io_context] { io_context.run(); });
    }
    IP_LOG_INFO("ironpulse is running — dashboard on http://localhost:{}", config.http_port);

    while (g_running) {
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }

    IP_LOG_INFO("Shutting down...");
    for (auto& poller : pollers) {
        poller->stop();
    }
    http_server.stop();
    ws_server_ptr = nullptr;
    ws_server->stop();
    asio::post(io_context, [&] { maintenance_timer.cancel(); });
    notifier.stop(std::chrono::seconds(5));
    if (wal) {
        wal->flush_all();
    }
    work_guard.reset();
    io_context.stop();
    for (auto& t : io_threads) {
        if (t.joinable())
            t.join();
    }
    if (exporter) {
        // After the I/O threads are gone no reading can arrive any more, so
        // everything queued is written out before the segment is sealed.
        exporter->stop();
        const auto stats = exporter->stats();
        IP_LOG_INFO("Export: {} reading(s) in {} batch(es), {} dropped",
                    stats.accepted,
                    stats.batches_written,
                    stats.dropped);
    }
    IP_LOG_INFO("Stopped cleanly");
    return 0;
}
