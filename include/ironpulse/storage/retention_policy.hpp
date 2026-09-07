#pragma once

#include <chrono>
#include <filesystem>
#include <fstream>

#include "ironpulse/storage/wal_writer.hpp"

namespace ironpulse::storage {

/// Rewrites a WAL file in place, keeping only records newer than
/// `now - max_age`. Intended to run periodically (e.g. once an hour) via
/// a background timer so WAL files don't grow unbounded.
///
/// Implementation reads the whole file, filters in memory, and rewrites —
/// simple and correct, and perfectly adequate at the file sizes a single
/// sensor's telemetry produces (a compacting/streaming rewrite would only
/// be worth the complexity at a much larger scale than this project
/// targets).
class RetentionPolicy {
public:
    explicit RetentionPolicy(std::chrono::milliseconds max_age) : max_age_(max_age) {}

    /// Returns the number of records retained after pruning.
    std::size_t prune(const std::filesystem::path& wal_file) const {
        auto records = WalReader::read_all(wal_file);
        const auto now = std::chrono::system_clock::now();

        std::vector<WalRecord> kept;
        kept.reserve(records.size());
        for (const auto& record : records) {
            const auto ts =
                std::chrono::system_clock::time_point(std::chrono::milliseconds(record.timestamp_ms));
            if ((now - ts) <= max_age_) {
                kept.push_back(record);
            }
        }

        if (kept.size() == records.size()) {
            return kept.size();  // nothing to prune, avoid a needless rewrite
        }

        std::ofstream out(wal_file, std::ios::binary | std::ios::trunc);
        out.write(reinterpret_cast<const char*>(kept.data()),
                  static_cast<std::streamsize>(kept.size() * sizeof(WalRecord)));
        return kept.size();
    }

private:
    std::chrono::milliseconds max_age_;
};

}  // namespace ironpulse::storage
