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

    /// Drops records older than `max_age` from every WAL file in the
    /// directory, so the log never grows without bound. Holds the writer's
    /// lock for the duration, so it cannot race with append()/flush, and
    /// rewrites each file via a temporary + rename so a crash mid-prune
    /// never leaves a truncated log behind. Returns the number of records
    /// removed.
    std::size_t prune_older_than(std::chrono::milliseconds max_age);

    /// Returns a sensor's persisted samples at or after `since`, oldest
    /// first — the full retained history, not just what fits in memory.
    /// Pending records are flushed first so the result is complete.
    [[nodiscard]] std::vector<Sample<double>> read_since(const std::string& sensor_id,
                                                         std::chrono::system_clock::time_point since);

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

inline std::vector<Sample<double>> WalWriter::read_since(const std::string& sensor_id,
                                                         std::chrono::system_clock::time_point since) {
    std::lock_guard lock(mutex_);
    if (auto it = pending_.find(sensor_id); it != pending_.end() && !it->second.empty()) {
        flush_locked(sensor_id, it->second);
    }
    const auto since_ms =
        std::chrono::duration_cast<std::chrono::milliseconds>(since.time_since_epoch()).count();
    std::vector<Sample<double>> samples;
    for (const auto& record : WalReader::read_all(file_for(sensor_id))) {
        if (record.timestamp_ms >= since_ms) {
            samples.push_back(Sample<double>{
                std::chrono::system_clock::time_point(std::chrono::milliseconds(record.timestamp_ms)),
                record.value});
        }
    }
    return samples;
}

inline std::size_t WalWriter::prune_older_than(std::chrono::milliseconds max_age) {
    std::lock_guard lock(mutex_);
    for (auto& [sensor_id, buffer] : pending_) {
        if (!buffer.empty()) {
            flush_locked(sensor_id, buffer);
        }
    }

    const auto cutoff_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                               (std::chrono::system_clock::now() - max_age).time_since_epoch())
                               .count();

    std::size_t removed = 0;
    std::error_code ec;
    for (const auto& entry : std::filesystem::directory_iterator(directory_, ec)) {
        if (!entry.is_regular_file() || entry.path().extension() != ".wal") {
            continue;
        }
        const auto records = WalReader::read_all(entry.path());
        std::vector<WalRecord> kept;
        kept.reserve(records.size());
        for (const auto& record : records) {
            if (record.timestamp_ms >= cutoff_ms) {
                kept.push_back(record);
            }
        }
        if (kept.size() == records.size()) {
            continue;
        }

        auto tmp = entry.path();
        tmp += ".tmp";
        {
            std::ofstream out(tmp, std::ios::binary | std::ios::trunc);
            out.write(reinterpret_cast<const char*>(kept.data()),
                      static_cast<std::streamsize>(kept.size() * sizeof(WalRecord)));
            if (!out) {
                std::filesystem::remove(tmp, ec);
                continue;
            }
        }
        std::filesystem::rename(tmp, entry.path(), ec);
        if (!ec) {
            removed += records.size() - kept.size();
        }
    }
    return removed;
}

}  // namespace ironpulse::storage
