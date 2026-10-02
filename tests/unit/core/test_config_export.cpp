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
    CHECK_THROWS_WITH(AppConfig::load_from_string(with_export(R"({"queue_capacity": 1024, "batch_size": 2048})")),
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
