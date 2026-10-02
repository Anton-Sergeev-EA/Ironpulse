#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>
#include <cstdlib>
#include <string>

#include "ironpulse/core/config.hpp"

using Catch::Matchers::ContainsSubstring;
using ironpulse::core::AppConfig;

namespace {

class EnvGuard {
public:
    EnvGuard(const char* name, const std::string& value) : name_(name) {
        setenv(name_, value.c_str(), 1);
    }
    ~EnvGuard() {
        unsetenv(name_);
    }
    EnvGuard(const EnvGuard&) = delete;
    EnvGuard& operator=(const EnvGuard&) = delete;

private:
    const char* name_;
};

std::string with_export(const std::string& export_json) {
    return R"({"devices": [], "export": )" + export_json + "}";
}

}  // namespace

TEST_CASE("Export is disabled by default with sensible defaults", "[config][export]") {
    const auto cfg = AppConfig::load_from_string(R"({"devices": []})");
    CHECK_FALSE(cfg.export_config.enabled);
    CHECK(cfg.export_config.directory == "export");
    CHECK(cfg.export_config.queue_capacity == 65536);
    CHECK(cfg.export_config.batch_size == 1000);
    CHECK(cfg.export_config.flush_interval_ms == 1000);
    CHECK(cfg.export_config.segment_max_mb == 64);
    CHECK(cfg.export_config.max_total_mb == 1024);
}

TEST_CASE("The export section is read from the config file", "[config][export]") {
    const auto cfg = AppConfig::load_from_string(with_export(R"({
        "enabled": true,
        "directory": "/var/lib/ironpulse/export",
        "queue_capacity": 4096,
        "batch_size": 500,
        "flush_interval_ms": 250,
        "segment_max_mb": 16
    })"));
    CHECK(cfg.export_config.enabled);
    CHECK(cfg.export_config.directory == "/var/lib/ironpulse/export");
    CHECK(cfg.export_config.queue_capacity == 4096);
    CHECK(cfg.export_config.batch_size == 500);
    CHECK(cfg.export_config.flush_interval_ms == 250);
    CHECK(cfg.export_config.segment_max_mb == 16);
}

TEST_CASE("Invalid export settings are rejected with the offending key", "[config][export]") {
    CHECK_THROWS_WITH(AppConfig::load_from_string(with_export(R"({"queue_capacity": 5000})")),
                      ContainsSubstring("export.queue_capacity") && ContainsSubstring("power of two"));
    CHECK_THROWS_WITH(AppConfig::load_from_string(with_export(R"({"queue_capacity": 8})")),
                      ContainsSubstring("export.queue_capacity"));
    CHECK_THROWS_WITH(
        AppConfig::load_from_string(with_export(R"({"queue_capacity": 1024, "batch_size": 2048})")),
        ContainsSubstring("export.batch_size"));
    CHECK_THROWS_WITH(AppConfig::load_from_string(with_export(R"({"flush_interval_ms": 0})")),
                      ContainsSubstring("export.flush_interval_ms"));
    CHECK_THROWS_WITH(AppConfig::load_from_string(with_export(R"({"enabled": "yes"})")),
                      ContainsSubstring("export.enabled"));
    CHECK_THROWS_WITH(AppConfig::load_from_string(with_export(R"({"enabled": true, "directory": ""})")),
                      ContainsSubstring("export.directory"));
    CHECK_THROWS_WITH(AppConfig::load_from_string(with_export("[]")), ContainsSubstring("export"));
}

TEST_CASE("IRONPULSE_EXPORT_* environment variables override the file", "[config][export][env]") {
    EnvGuard enabled("IRONPULSE_EXPORT_ENABLED", "true");
    EnvGuard dir("IRONPULSE_EXPORT_DIR", "/data/export");
    const auto cfg = AppConfig::load_from_string(with_export(R"({"enabled": false, "directory": "local"})"));
    CHECK(cfg.export_config.enabled);
    CHECK(cfg.export_config.directory == "/data/export");
}

TEST_CASE("The export size limit must leave room for two segments, or be 0", "[config][export]") {
    CHECK(AppConfig::load_from_string(with_export(R"({"segment_max_mb": 8, "max_total_mb": 16})"))
              .export_config.max_total_mb == 16);
    CHECK(AppConfig::load_from_string(with_export(R"({"max_total_mb": 0})")).export_config.max_total_mb == 0);
    CHECK_THROWS_WITH(
        AppConfig::load_from_string(with_export(R"({"segment_max_mb": 64, "max_total_mb": 100})")),
        ContainsSubstring("export.max_total_mb") && ContainsSubstring("128"));
    CHECK_THROWS_WITH(AppConfig::load_from_string(with_export(R"({"max_total_mb": -1})")),
                      ContainsSubstring("export.max_total_mb"));
}

TEST_CASE("IRONPULSE_EXPORT_MAX_MB and IRONPULSE_EXPORT_SEGMENT_MB override the file",
          "[config][export][env]") {
    EnvGuard max_mb("IRONPULSE_EXPORT_MAX_MB", "200");
    EnvGuard segment_mb("IRONPULSE_EXPORT_SEGMENT_MB", "8");
    const auto cfg =
        AppConfig::load_from_string(with_export(R"({"max_total_mb": 5000, "segment_max_mb": 64})"));
    CHECK(cfg.export_config.max_total_mb == 200);
    CHECK(cfg.export_config.segment_max_mb == 8);
}

TEST_CASE("Environment overrides are validated too", "[config][export][env]") {
    EnvGuard max_mb("IRONPULSE_EXPORT_MAX_MB", "50");  // below 2 x the default 64 MB segment
    CHECK_THROWS_WITH(AppConfig::load_from_string(R"({"devices": []})"),
                      ContainsSubstring("export.max_total_mb"));
}
