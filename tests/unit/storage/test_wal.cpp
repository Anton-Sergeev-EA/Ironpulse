#include <atomic>
#include <catch2/catch_test_macros.hpp>
#include <filesystem>

#include "ironpulse/storage/retention_policy.hpp"
#include "ironpulse/storage/wal_writer.hpp"

using namespace ironpulse::storage;

namespace {
std::filesystem::path make_temp_dir() {
    static std::atomic<int> counter{0};
    auto unique_suffix = std::chrono::steady_clock::now().time_since_epoch().count();
    auto dir = std::filesystem::temp_directory_path() /
               ("ironpulse_wal_test_" + std::to_string(unique_suffix) + "_" + std::to_string(counter++));
    std::filesystem::create_directories(dir);
    return dir;
}
}  // namespace

TEST_CASE("WalWriter persists samples that WalReader can read back", "[wal]") {
    auto dir = make_temp_dir();
    {
        WalWriter writer(dir, /*flush_batch_size=*/1);  // flush immediately for the test
        auto now = std::chrono::system_clock::now();
        writer.append("sensor_x", 1.0, now);
        writer.append("sensor_x", 2.0, now + std::chrono::seconds(1));
        writer.append("sensor_x", 3.0, now + std::chrono::seconds(2));
    }

    auto records = WalReader::read_all(dir / "sensor_x.wal");
    REQUIRE(records.size() == 3);
    CHECK(records[0].value == 1.0);
    CHECK(records[1].value == 2.0);
    CHECK(records[2].value == 3.0);

    std::filesystem::remove_all(dir);
}

TEST_CASE("WalWriter buffers writes until the flush batch size is reached", "[wal]") {
    auto dir = make_temp_dir();
    WalWriter writer(dir, /*flush_batch_size=*/10);
    auto now = std::chrono::system_clock::now();
    writer.append("sensor_y", 1.0, now);
    writer.append("sensor_y", 2.0, now);

    // Not flushed yet — file shouldn't exist, or should be empty.
    auto records_before = WalReader::read_all(dir / "sensor_y.wal");
    CHECK(records_before.empty());

    writer.flush_all();
    auto records_after = WalReader::read_all(dir / "sensor_y.wal");
    CHECK(records_after.size() == 2);

    std::filesystem::remove_all(dir);
}

TEST_CASE("WalReader::replay_into loads samples into a RingBuffer", "[wal]") {
    auto dir = make_temp_dir();
    {
        WalWriter writer(dir, 1);
        auto now = std::chrono::system_clock::now();
        writer.append("sensor_z", 10.0, now);
        writer.append("sensor_z", 20.0, now);
    }

    RingBuffer<double> buffer(10);
    WalReader::replay_into(dir / "sensor_z.wal", buffer);

    CHECK(buffer.size() == 2);
    auto latest = buffer.latest();
    REQUIRE(latest.has_value());
    CHECK(latest->value == 20.0);

    std::filesystem::remove_all(dir);
}

TEST_CASE("WalReader::replay_into skips records older than max_age", "[wal]") {
    auto dir = make_temp_dir();
    {
        WalWriter writer(dir, 1);
        auto old_ts = std::chrono::system_clock::now() - std::chrono::hours(48);
        auto recent_ts = std::chrono::system_clock::now();
        writer.append("sensor_w", 1.0, old_ts);
        writer.append("sensor_w", 2.0, recent_ts);
    }

    RingBuffer<double> buffer(10);
    WalReader::replay_into(dir / "sensor_w.wal", buffer, std::chrono::hours(1));

    CHECK(buffer.size() == 1);
    auto latest = buffer.latest();
    REQUIRE(latest.has_value());
    CHECK(latest->value == 2.0);

    std::filesystem::remove_all(dir);
}

TEST_CASE("RetentionPolicy prunes records older than max_age", "[wal][retention]") {
    auto dir = make_temp_dir();
    auto file = dir / "sensor_r.wal";
    {
        WalWriter writer(dir, 1);
        auto old_ts = std::chrono::system_clock::now() - std::chrono::hours(200);
        auto recent_ts = std::chrono::system_clock::now();
        writer.append("sensor_r", 1.0, old_ts);
        writer.append("sensor_r", 2.0, old_ts);
        writer.append("sensor_r", 3.0, recent_ts);
    }

    RetentionPolicy policy(std::chrono::hours(168));  // 7 days
    auto kept = policy.prune(file);

    CHECK(kept == 1);
    auto records = WalReader::read_all(file);
    REQUIRE(records.size() == 1);
    CHECK(records[0].value == 3.0);

    std::filesystem::remove_all(dir);
}

TEST_CASE("RetentionPolicy leaves a file untouched when nothing is prunable", "[wal][retention]") {
    auto dir = make_temp_dir();
    auto file = dir / "sensor_s.wal";
    {
        WalWriter writer(dir, 1);
        writer.append("sensor_s", 1.0, std::chrono::system_clock::now());
    }

    RetentionPolicy policy(std::chrono::hours(168));
    auto kept = policy.prune(file);

    CHECK(kept == 1);

    std::filesystem::remove_all(dir);
}
