#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>
#include <cstdlib>

#include "ironpulse/core/config.hpp"

using Catch::Matchers::ContainsSubstring;
using ironpulse::core::AppConfig;
using ironpulse::core::DataType;
using ironpulse::core::RegisterType;
using ironpulse::core::WordOrder;

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

std::string device_with(const std::string& sensors_json) {
    return R"({"devices": [{"id": "plc", "host": "10.0.0.5", "sensors": )" + sensors_json + "}]}";
}

}  // namespace

TEST_CASE("A full sensor definition is parsed", "[config][sensors]") {
    const auto cfg = AppConfig::load_from_string(device_with(R"([{
        "id": "winding_temp",
        "name": "Winding temperature",
        "unit": "°C",
        "register_type": "input",
        "address": 100,
        "data_type": "float32",
        "word_order": "little",
        "scale": 0.5,
        "offset": -10,
        "limits": {"low": 5, "high": 95},
        "detection": {
            "detectors": [
                {"type": "zscore", "window": 30, "threshold": 4},
                {"type": "cusum", "mean": 65, "stddev": 1.5}
            ],
            "votes_required": 2,
            "cooldown_seconds": 120
        }
    }])"));

    REQUIRE(cfg.devices.size() == 1);
    REQUIRE(cfg.devices[0].sensors.size() == 1);
    const auto& s = cfg.devices[0].sensors[0];
    CHECK(s.id == "winding_temp");
    CHECK(s.name == "Winding temperature");
    CHECK(s.unit == "°C");
    CHECK(s.register_type == RegisterType::input);
    CHECK(s.address == 100);
    CHECK(s.data_type == DataType::float32);
    CHECK(s.word_order == WordOrder::little);
    CHECK(s.scale == 0.5);
    CHECK(s.offset == -10.0);
    CHECK(s.limits.low == 5.0);
    CHECK(s.limits.high == 95.0);
    REQUIRE(s.detection.detectors.size() == 2);
    CHECK(s.detection.detectors[0].window == 30);
    CHECK(s.detection.detectors[1].type == "cusum");
    CHECK(s.detection.detectors[1].threshold == 5.0);  // cusum default
    CHECK(s.detection.votes_required == 2);
    CHECK(s.detection.cooldown_seconds == 120);
}

TEST_CASE("A device without sensors keeps the original one-register behaviour", "[config][sensors]") {
    const auto cfg =
        AppConfig::load_from_string(R"({"devices": [{"id": "legacy", "host": "h", "unit_id": 3}]})");
    const auto& d = cfg.devices.at(0);
    REQUIRE(d.sensors.size() == 1);
    CHECK(d.sensors[0].id == "legacy");
    CHECK(d.sensors[0].address == 0);
    CHECK(d.sensors[0].data_type == DataType::uint16);
    CHECK(d.sensors[0].detection.detectors.size() == 2);  // zscore + ewma defaults
    CHECK(d.unit_id == 3);
}

TEST_CASE("Invalid configurations are rejected with a precise message", "[config][validation]") {
    SECTION("ids that could escape the data directory") {
        CHECK_THROWS_WITH(AppConfig::load_from_string(device_with(R"([{"id": "../etc/passwd"}])")),
                          ContainsSubstring("devices[0].sensors[0].id"));
    }
    SECTION("duplicate sensor ids across devices") {
        const std::string json = R"({"devices": [
            {"id": "a", "host": "h", "sensors": [{"id": "t"}]},
            {"id": "b", "host": "h", "sensors": [{"id": "t"}]}]})";
        CHECK_THROWS_WITH(AppConfig::load_from_string(json), ContainsSubstring("duplicate sensor id 't'"));
    }
    SECTION("out-of-range port instead of silent truncation") {
        CHECK_THROWS_WITH(
            AppConfig::load_from_string(R"({"devices": [{"id": "a", "host": "h", "port": 70000}]})"),
            ContainsSubstring("out of range"));
    }
    SECTION("unknown data type lists the valid ones") {
        CHECK_THROWS_WITH(
            AppConfig::load_from_string(device_with(R"([{"id": "t", "data_type": "float64"}])")),
            ContainsSubstring("float32"));
    }
    SECTION("limits in the wrong order") {
        CHECK_THROWS_WITH(
            AppConfig::load_from_string(device_with(R"([{"id": "t", "limits": {"low": 10, "high": 5}}])")),
            ContainsSubstring("'low' must be less than 'high'"));
    }
    SECTION("a quorum larger than the detector list") {
        CHECK_THROWS_WITH(
            AppConfig::load_from_string(device_with(
                R"([{"id": "t", "detection": {"detectors": [{"type": "zscore"}], "votes_required": 2}}])")),
            ContainsSubstring("votes_required"));
    }
    SECTION("a 32-bit value past the last register") {
        CHECK_THROWS(AppConfig::load_from_string(
            device_with(R"([{"id": "t", "address": 65535, "data_type": "uint32"}])")));
    }
    SECTION("malformed JSON") {
        CHECK_THROWS_WITH(AppConfig::load_from_string("{not json"), ContainsSubstring("invalid JSON"));
    }
    SECTION("unknown log level") {
        CHECK_THROWS_WITH(AppConfig::load_from_string(R"({"log_level": "verbose"})"),
                          ContainsSubstring("log_level"));
    }
}

TEST_CASE("${VAR} placeholders are expanded from the environment", "[config][env]") {
    EnvGuard host("IP_TEST_PLC_HOST", "192.168.7.20");
    EnvGuard token("IP_TEST_TOKEN", "s3cret");

    const auto cfg = AppConfig::load_from_string(
        R"({"api_token": "${IP_TEST_TOKEN}", "devices": [{"id": "a", "host": "${IP_TEST_PLC_HOST}"}]})");

    CHECK(cfg.api_token == "s3cret");
    CHECK(cfg.devices[0].host == "192.168.7.20");
    CHECK(ironpulse::core::expand_env("x-${IP_TEST_UNSET_VAR}-y") == "x--y");
}

TEST_CASE("Notification channels come from the file and from environment shortcuts",
          "[config][notifications]") {
    EnvGuard bot("IRONPULSE_TELEGRAM_BOT_TOKEN", "123:abc");
    EnvGuard chat("IRONPULSE_TELEGRAM_CHAT_ID", "-1001");
    EnvGuard lang("IRONPULSE_NOTIFY_LANGUAGE", "de");

    const auto cfg = AppConfig::load_from_string(R"({"notifications": {
        "min_severity": "critical",
        "channels": [{"type": "webhook", "url": "https://example.org/hook", "headers": {"X-Key": "k"}}]
    }})");

    CHECK(cfg.notifications.language == "de");
    CHECK(cfg.notifications.min_severity == ironpulse::core::Severity::critical);
    REQUIRE(cfg.notifications.channels.size() == 2);
    CHECK(cfg.notifications.channels[0].type == "webhook");
    CHECK(cfg.notifications.channels[0].headers.at("X-Key") == "k");
    CHECK(cfg.notifications.channels[1].type == "telegram");
    CHECK(cfg.notifications.channels[1].chat_id == "-1001");
}

TEST_CASE("A channel left without credentials is disabled with a warning, not a crash",
          "[config][notifications]") {
    const auto cfg = AppConfig::load_from_string(
        R"({"notifications": {"channels": [{"type": "telegram", "bot_token": "${IP_TEST_UNSET_BOT}", "chat_id": "1"}]}})");

    CHECK(cfg.notifications.channels.empty());
    REQUIRE(cfg.warnings.size() == 1);
    CHECK_THAT(cfg.warnings[0], ContainsSubstring("telegram"));
}

TEST_CASE("is_valid_id accepts safe identifiers only", "[config]") {
    using ironpulse::core::is_valid_id;
    CHECK(is_valid_id("transformer_temp-01.a"));
    CHECK_FALSE(is_valid_id(""));
    CHECK_FALSE(is_valid_id(".hidden"));
    CHECK_FALSE(is_valid_id("a/b"));
    CHECK_FALSE(is_valid_id("with space"));
    CHECK_FALSE(is_valid_id(std::string(65, 'x')));
}
