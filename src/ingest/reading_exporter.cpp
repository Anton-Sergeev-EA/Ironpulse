#include "ironpulse/ingest/reading_exporter.hpp"

#include <utility>

namespace ironpulse::ingest {

std::vector<TagInfo> tags_from_devices(const std::vector<core::ModbusDeviceConfig>& devices) {
    std::vector<TagInfo> tags;
    std::uint32_t next_id = 1;
    for (const auto& device : devices) {
        for (const auto& sensor : device.sensors) {
            tags.push_back(TagInfo{
                .id = next_id++,
                .sensor_id = sensor.id,
                .device_id = device.id,
                .name = sensor.name,
                .unit = sensor.unit,
            });
        }
    }
    return tags;
}

ReadingExporter::ReadingExporter(const core::ExportConfig& config, std::vector<TagInfo> tags) {
    for (const auto& tag : tags) {
        tag_ids_.emplace(tag.sensor_id, tag.id);
    }
    writer_ = std::make_unique<SegmentWriter>(
        SegmentWriterOptions{
            .directory = config.directory,
            .max_segment_bytes = std::uint64_t{config.segment_max_mb} * 1024 * 1024,
        },
        std::move(tags));
    ingestor_ = std::make_unique<Ingestor>(
        IngestorOptions{
            .queue_capacity = config.queue_capacity,
            .batch_size = config.batch_size,
            .flush_interval = std::chrono::milliseconds(config.flush_interval_ms),
        },
        [writer = writer_.get()](std::span<const std::uint8_t> batch) { writer->write_batch(batch); });
}

ReadingExporter::~ReadingExporter() {
    stop();
}

void ReadingExporter::start() {
    std::lock_guard lock(producer_mutex_);
    if (accepting_) {
        return;
    }
    ingestor_->start();
    accepting_ = true;
}

void ReadingExporter::stop() noexcept {
    {
        // After this block no producer is inside the queue, and none will
        // enter it again, so the Ingestor may drain and stop safely.
        std::lock_guard lock(producer_mutex_);
        if (!accepting_) {
            return;
        }
        accepting_ = false;
    }
    ingestor_->stop();
    writer_->close();
}

bool ReadingExporter::submit(const std::string& sensor_id,
                             double value,
                             std::chrono::system_clock::time_point timestamp) {
    const auto tag = tag_ids_.find(sensor_id);
    if (tag == tag_ids_.end()) {
        unknown_sensor_.fetch_add(1, std::memory_order_relaxed);
        return false;
    }
    const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(timestamp.time_since_epoch()).count();
    const TelemetrySample sample{
        .timestamp_ms = static_cast<std::uint64_t>(ms < 0 ? 0 : ms),
        .value = value,
        .tag_id = tag->second,
        .quality = 0,
    };

    std::lock_guard lock(producer_mutex_);
    return accepting_ && ingestor_->submit(sample);
}

ExportStats ReadingExporter::stats() const noexcept {
    const auto counters = ingestor_->counters();
    return ExportStats{
        .accepted = counters.ingested,
        .dropped = counters.dropped,
        .unknown_sensor = unknown_sensor_.load(std::memory_order_relaxed),
        .batches_written = counters.batches_sent,
        .batches_failed = counters.batches_failed,
        .bytes_written = writer_->bytes_written(),
        .segments_completed = writer_->segments_completed(),
        .queue_depth = ingestor_->queue_depth(),
    };
}

}  // namespace ironpulse::ingest
