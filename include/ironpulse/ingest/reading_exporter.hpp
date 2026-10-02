#pragma once

#include <atomic>
#include <chrono>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include "ironpulse/core/config.hpp"
#include "ironpulse/ingest/ingestor.hpp"
#include "ironpulse/ingest/segment.hpp"

namespace ironpulse::ingest {

/// Counters for the /metrics endpoint.
struct ExportStats {
    std::uint64_t accepted = 0;        // readings queued for export
    std::uint64_t dropped = 0;         // readings lost because the queue was full
    std::uint64_t unknown_sensor = 0;  // readings for a sensor missing from the tag table
    std::uint64_t batches_written = 0;
    std::uint64_t batches_failed = 0;  // batches lost to an I/O error
    std::uint64_t bytes_written = 0;
    std::uint64_t segments_completed = 0;
    std::uint64_t segments_deleted = 0;  // removed to stay within max_total_mb
    std::size_t queue_depth = 0;
};

/// Assigns tag ids 1..N to the configured sensors, in configuration order.
[[nodiscard]] std::vector<TagInfo> tags_from_devices(const std::vector<core::ModbusDeviceConfig>& devices);

/// Streams every sensor reading into compact, CRC-checked segment files
/// for bulk use outside Ironpulse — a data lake, a historian, offline
/// analysis or model training. It is a separate path from the per-sensor
/// WAL, which exists to serve the dashboard's history after a restart.
///
/// Thread model: submit() may be called from any number of threads (the
/// polling strands). They take turns through a short critical section in
/// front of the SPSC queue — a few nanoseconds of bookkeeping — while all
/// encoding and disk I/O happens on the Ingestor's own thread. A slow or
/// stalled disk therefore fills the queue and drops (counted) export
/// samples; it can never hold up polling, alerting or the dashboard.
class ReadingExporter {
public:
    ReadingExporter(const core::ExportConfig& config, std::vector<TagInfo> tags);
    ~ReadingExporter();

    ReadingExporter(const ReadingExporter&) = delete;
    ReadingExporter& operator=(const ReadingExporter&) = delete;

    void start();
    /// Stops accepting readings, writes out everything queued and closes
    /// the current segment. Idempotent.
    void stop() noexcept;

    /// Queues one reading. Returns false if it was not queued (exporter
    /// stopped, unknown sensor or queue full).
    bool submit(const std::string& sensor_id, double value, std::chrono::system_clock::time_point timestamp);

    [[nodiscard]] ExportStats stats() const noexcept;

    [[nodiscard]] std::size_t recovered_segments() const noexcept {
        return writer_->recovered_on_startup();
    }

private:
    std::unordered_map<std::string, std::uint32_t> tag_ids_;  // immutable after construction
    std::unique_ptr<SegmentWriter> writer_;
    std::unique_ptr<Ingestor> ingestor_;

    std::mutex producer_mutex_;  // serialises producers in front of the SPSC queue
    bool accepting_ = false;     // guarded by producer_mutex_
    std::atomic<std::uint64_t> unknown_sensor_{0};
};

}  // namespace ironpulse::ingest
