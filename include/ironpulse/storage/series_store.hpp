#pragma once

#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>

#include "ironpulse/storage/ring_buffer.hpp"
#include "ironpulse/storage/wal_writer.hpp"

namespace ironpulse::storage {

/// Registry mapping sensor IDs to their in-memory ring buffers.
///
/// This is the entry point the protocol layer writes into, and the
/// analytics layer reads from. When constructed with a WalWriter, every
/// recorded sample is also appended to that sensor's write-ahead log, and
/// a sensor's history is replayed from its WAL file the first time it is
/// seen after a restart — see docs/adr/0002-storage-format.md.
class SeriesStore {
public:
    explicit SeriesStore(std::size_t buffer_capacity_per_series = 4096)
        : default_capacity_(buffer_capacity_per_series) {}

    /// Enables WAL persistence: new writes are appended to disk, and a
    /// sensor's prior history is replayed into its buffer the first time
    /// it's touched after construction. `replay_max_age`, if set, discards
    /// WAL records older than that on replay (paired with a
    /// RetentionPolicy pruning the files themselves periodically).
    SeriesStore(std::size_t buffer_capacity_per_series,
                std::shared_ptr<WalWriter> wal,
                std::optional<std::chrono::milliseconds> replay_max_age = std::nullopt)
        : default_capacity_(buffer_capacity_per_series),
          wal_(std::move(wal)),
          replay_max_age_(replay_max_age) {}

    void record(const std::string& sensor_id,
                double value,
                std::chrono::system_clock::time_point timestamp = std::chrono::system_clock::now()) {
        auto& buffer = get_or_create(sensor_id);
        buffer.push(value, timestamp);
        if (wal_) {
            wal_->append(sensor_id, value, timestamp);
        }
    }

    [[nodiscard]] std::optional<Sample<double>> latest(const std::string& sensor_id) const {
        std::lock_guard lock(mutex_);
        auto it = series_.find(sensor_id);
        if (it == series_.end()) {
            return std::nullopt;
        }
        return it->second->latest();
    }

    [[nodiscard]] std::vector<Sample<double>> recent(const std::string& sensor_id, std::size_t count) const {
        std::lock_guard lock(mutex_);
        auto it = series_.find(sensor_id);
        if (it == series_.end()) {
            return {};
        }
        return it->second->recent(count);
    }

    /// Returns the set of sensor IDs currently tracked (i.e. that have
    /// received at least one `record()` call, or were replayed from WAL).
    [[nodiscard]] std::vector<std::string> sensor_ids() const {
        std::lock_guard lock(mutex_);
        std::vector<std::string> ids;
        ids.reserve(series_.size());
        for (const auto& [id, _] : series_) {
            ids.push_back(id);
        }
        return ids;
    }

private:
    RingBuffer<double>& get_or_create(const std::string& sensor_id) {
        std::lock_guard lock(mutex_);
        auto it = series_.find(sensor_id);
        if (it != series_.end()) {
            return *it->second;
        }
        auto [inserted, _] =
            series_.emplace(sensor_id, std::make_unique<RingBuffer<double>>(default_capacity_));

        if (wal_) {
            WalReader::replay_into(wal_->file_for(sensor_id), *inserted->second, replay_max_age_);
        }

        return *inserted->second;
    }

    mutable std::mutex mutex_;
    std::size_t default_capacity_;
    std::unordered_map<std::string, std::unique_ptr<RingBuffer<double>>> series_;
    std::shared_ptr<WalWriter> wal_;
    std::optional<std::chrono::milliseconds> replay_max_age_;
};

}  // namespace ironpulse::storage
