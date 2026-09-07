#pragma once

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "ironpulse/storage/ring_buffer.hpp"

namespace ironpulse::storage {

/// On-disk record: 8 bytes epoch-millis timestamp + 8 bytes double value,
/// fixed width, no framing needed since records are constant-size.
struct WalRecord {
    std::int64_t timestamp_ms;
    double value;
};

/// Appends samples to a per-sensor, append-only binary log so recent
/// history survives a process restart. Writes are buffered in memory and
/// flushed to disk in batches (by count or by a background timer driven
/// externally via `flush()`), trading a small durability window for
/// throughput — acceptable for telemetry, where losing the last
/// fraction-of-a-second of samples on an unclean shutdown is not
/// catastrophic.
///
/// One file per sensor keeps the format trivial and makes retention
/// pruning (see RetentionPolicy) a simple per-file rewrite.
class WalWriter {
public:
    WalWriter(std::filesystem::path directory, std::size_t flush_batch_size = 32)
        : directory_(std::move(directory)), flush_batch_size_(flush_batch_size) {
        std::filesystem::create_directories(directory_);
    }

    void append(const std::string& sensor_id, double value, std::chrono::system_clock::time_point timestamp) {
        const auto ms =
            std::chrono::duration_cast<std::chrono::milliseconds>(timestamp.time_since_epoch()).count();

        std::lock_guard lock(mutex_);
        auto& buffer = pending_[sensor_id];
        buffer.push_back(WalRecord{ms, value});
        if (buffer.size() >= flush_batch_size_) {
            flush_locked(sensor_id, buffer);
        }
    }

    /// Forces all buffered records to disk. Call periodically (e.g. from a
    /// timer) and on shutdown so nothing sits unflushed indefinitely.
    void flush_all() {
        std::lock_guard lock(mutex_);
        for (auto& [sensor_id, buffer] : pending_) {
            if (!buffer.empty()) {
                flush_locked(sensor_id, buffer);
            }
        }
    }

    [[nodiscard]] std::filesystem::path file_for(const std::string& sensor_id) const {
        return directory_ / (sensor_id + ".wal");
    }

private:
    void flush_locked(const std::string& sensor_id, std::vector<WalRecord>& buffer) {
        std::ofstream out(file_for(sensor_id), std::ios::binary | std::ios::app);
        out.write(reinterpret_cast<const char*>(buffer.data()),
                  static_cast<std::streamsize>(buffer.size() * sizeof(WalRecord)));
        buffer.clear();
    }

    std::filesystem::path directory_;
    std::size_t flush_batch_size_;
    std::mutex mutex_;
    std::unordered_map<std::string, std::vector<WalRecord>> pending_;
};

/// Reads a sensor's WAL file back into a RingBuffer, e.g. on startup so
/// recent history survives a restart. Records older than `max_age`
/// (if provided) are skipped.
class WalReader {
public:
    static std::vector<WalRecord> read_all(const std::filesystem::path& file) {
        std::vector<WalRecord> records;
        std::ifstream in(file, std::ios::binary);
        if (!in.is_open()) {
            return records;
        }

        WalRecord record{};
        while (in.read(reinterpret_cast<char*>(&record), sizeof(WalRecord))) {
            records.push_back(record);
        }
        return records;
    }

    static void replay_into(const std::filesystem::path& file,
                            RingBuffer<double>& buffer,
                            std::optional<std::chrono::milliseconds> max_age = std::nullopt) {
        const auto records = read_all(file);
        const auto now = std::chrono::system_clock::now();

        for (const auto& record : records) {
            const auto ts =
                std::chrono::system_clock::time_point(std::chrono::milliseconds(record.timestamp_ms));
            if (max_age && (now - ts) > *max_age) {
                continue;
            }
            buffer.push(record.value, ts);
        }
    }
};

}  // namespace ironpulse::storage
