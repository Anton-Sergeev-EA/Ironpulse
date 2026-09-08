#include <catch2/catch_test_macros.hpp>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>

#include "ironpulse/core/config.hpp"

using ironpulse::core::AppConfig;

namespace {

std::filesystem::path write_temp_config(const std::string& json_content) {
    auto path = std::filesystem::temp_directory_path() /
                ("ironpulse_config_test_" +
                 std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + ".json");
    std::ofstream out(path);
    out << json_content;
    return path;
}

/// RAII guard so a test that sets an env var can't leak it into later
/// tests even if an assertion throws partway through.
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

constexpr const char* kMinimalConfig = R"({
    "http_port": 8080,
    "ws_port": 8081,
    "devices": []
})";

}  // namespace

TEST_CASE("AppConfig::load_from_file reads values from the JSON file", "[config]") {
    auto path = write_temp_config(kMinimalConfig);
    auto cfg = AppConfig::load_from_file(path.string());

    CHECK(cfg.http_port == 8080);
    CHECK(cfg.ws_port == 8081);

    std::filesystem::remove(path);
}

TEST_CASE("AppConfig::load_from_file throws for a missing file", "[config]") {
    CHECK_THROWS_AS(AppConfig::load_from_file("/nonexistent/path/config.json"), std::runtime_error);
}

TEST_CASE("IRONPULSE_HTTP_PORT environment variable overrides the file value", "[config][env]") {
    auto path = write_temp_config(kMinimalConfig);
    EnvGuard guard("IRONPULSE_HTTP_PORT", "9090");

    auto cfg = AppConfig::load_from_file(path.string());

    CHECK(cfg.http_port == 9090);
    CHECK(cfg.ws_port == 8081);  // untouched: no env var set for this one

    std::filesystem::remove(path);
}

TEST_CASE("IRONPULSE_WS_PORT and IRONPULSE_HTTP_PORT can both be overridden together", "[config][env]") {
    auto path = write_temp_config(kMinimalConfig);
    EnvGuard http_guard("IRONPULSE_HTTP_PORT", "9090");
    EnvGuard ws_guard("IRONPULSE_WS_PORT", "9091");

    auto cfg = AppConfig::load_from_file(path.string());

    CHECK(cfg.http_port == 9090);
    CHECK(cfg.ws_port == 9091);

    std::filesystem::remove(path);
}

TEST_CASE("IRONPULSE_PERSISTENCE_ENABLED accepts common truthy spellings", "[config][env]") {
    auto path = write_temp_config(kMinimalConfig);
    {
        EnvGuard guard("IRONPULSE_PERSISTENCE_ENABLED", "true");
        auto cfg = AppConfig::load_from_file(path.string());
        CHECK(cfg.persistence_enabled == true);
    }
    {
        EnvGuard guard("IRONPULSE_PERSISTENCE_ENABLED", "1");
        auto cfg = AppConfig::load_from_file(path.string());
        CHECK(cfg.persistence_enabled == true);
    }
    std::filesystem::remove(path);
}

TEST_CASE("An unset environment variable leaves the file's value untouched", "[config][env]") {
    auto path = write_temp_config(kMinimalConfig);
    auto cfg = AppConfig::load_from_file(path.string());

    CHECK(cfg.http_port == 8080);
    CHECK(cfg.ws_port == 8081);

    std::filesystem::remove(path);
}
