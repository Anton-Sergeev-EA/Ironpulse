#include <atomic>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>
#include <chrono>
#include <mutex>
#include <nlohmann/json.hpp>
#include <thread>
#include <vector>

#include "ironpulse/notify/messages.hpp"
#include "ironpulse/notify/notifier.hpp"

using Catch::Matchers::ContainsSubstring;
using Catch::Matchers::StartsWith;
using ironpulse::core::AlertKind;
using ironpulse::core::AnomalyEvent;
using ironpulse::core::NotificationChannelConfig;
using ironpulse::core::NotificationsConfig;
using ironpulse::core::Severity;
using namespace ironpulse::notify;

namespace {

AnomalyEvent limit_alert() {
    AnomalyEvent a;
    a.sensor_id = "winding_temp";
    a.kind = AlertKind::limit;
    a.severity = Severity::critical;
    a.value = 142.0;
    a.limit = 90.0;
    a.above_limit = true;
    a.timestamp = std::chrono::system_clock::time_point(std::chrono::seconds(1'790'000'000));
    return a;
}

AnomalyEvent statistical_alert() {
    AnomalyEvent a;
    a.sensor_id = "vibration";
    a.kind = AlertKind::statistical;
    a.severity = Severity::warning;
    a.value = 7.25;
    a.votes = 2;
    a.detectors_total = 3;
    a.timestamp = std::chrono::system_clock::now();
    return a;
}

const SensorInfo kWinding{"Winding temperature", "°C", "transformer_1"};

NotificationChannelConfig channel(const std::string& type) {
    NotificationChannelConfig c;
    c.type = type;
    c.url = "https://hooks.example.org/x";
    c.bot_token = "123:ABC";
    c.chat_id = "-100";
    return c;
}

}  // namespace

TEST_CASE("format_value keeps at most two decimals without trailing zeros", "[notify]") {
    CHECK(format_value(142.0) == "142");
    CHECK(format_value(7.25) == "7.25");
    CHECK(format_value(0.1) == "0.1");
    CHECK(format_value(-3.456) == "-3.46");
    CHECK(format_value(-0.001) == "0");
}

TEST_CASE("Alert texts are localized and include value, limit and unit", "[notify][i18n]") {
    const auto ru = describe_alert(limit_alert(), kWinding, "ru", "https://scada.example.org");
    CHECK_THAT(ru.text, ContainsSubstring("КРИТИЧНО"));
    CHECK_THAT(ru.text, ContainsSubstring("Winding temperature: значение 142 °C выше предела 90 °C"));
    CHECK_THAT(ru.text, ContainsSubstring("Устройство: transformer_1"));
    CHECK_THAT(ru.text, ContainsSubstring("2026-09-21 "));
    CHECK_THAT(ru.text, ContainsSubstring("Дашборд: https://scada.example.org"));

    const auto en = describe_alert(statistical_alert(), {"Bearing vibration", "mm/s", "pump"}, "en", "");
    CHECK_THAT(en.text, ContainsSubstring("WARNING"));
    CHECK_THAT(en.text, ContainsSubstring("confirmed by 2 of 3 detectors, current value 7.25 mm/s"));
    CHECK_THAT(en.text, !ContainsSubstring("Dashboard"));
}

TEST_CASE("Every supported language has its own alert and device texts", "[notify][i18n]") {
    const auto russian = describe_alert(limit_alert(), kWinding, "ru", "").text;
    for (const std::string lang : {"en", "zh", "hi", "es", "fr", "de", "it"}) {
        const auto alert = describe_alert(limit_alert(), kWinding, lang, "");
        const auto offline = describe_device_status("plc", false, std::chrono::system_clock::now(), lang, "");
        CAPTURE(lang);
        CHECK(alert.text != russian);
        CHECK_THAT(alert.text, ContainsSubstring("142 °C"));
        CHECK_THAT(offline.text, ContainsSubstring("plc"));
        CHECK(offline.severity == Severity::critical);
    }
    // Unknown codes fall back to Russian rather than producing nothing.
    CHECK(describe_alert(limit_alert(), kWinding, "xx", "").text == russian);
}

TEST_CASE("parse_url handles schemes, ports, paths and IPv6", "[notify][url]") {
    const auto a = parse_url("https://api.telegram.org/bot1:x/sendMessage");
    REQUIRE(a);
    CHECK(a->scheme == "https");
    CHECK(a->host == "api.telegram.org");
    CHECK(a->port == 443);
    CHECK(a->path == "/bot1:x/sendMessage");

    const auto b = parse_url("http://10.0.0.2:8080");
    REQUIRE(b);
    CHECK(b->port == 8080);
    CHECK(b->path == "/");

    const auto c = parse_url("http://[::1]:9000/hook?x=1");
    REQUIRE(c);
    CHECK(c->host == "::1");
    CHECK(c->port == 9000);
    CHECK(c->path == "/hook?x=1");

    CHECK_FALSE(parse_url("ftp://example.org"));
    CHECK_FALSE(parse_url("example.org/hook"));
    CHECK_FALSE(parse_url("https://user:pass@example.org/"));
    CHECK_FALSE(parse_url("http://host:99999/"));
}

TEST_CASE("Each channel builds its own request format", "[notify][channels]") {
    const auto n = describe_alert(limit_alert(), kWinding, "en", "https://dash");

    const auto tg = build_request(channel("telegram"), n, "en", "https://dash");
    // `url` on a telegram channel overrides the Bot API base.
    CHECK(tg.url == "https://hooks.example.org/x/bot123:ABC/sendMessage");
    const auto tg_body = nlohmann::json::parse(tg.body);
    CHECK(tg_body["chat_id"] == "-100");
    CHECK(tg_body["text"] == n.text);

    auto default_tg = channel("telegram");
    default_tg.url.clear();
    CHECK_THAT(build_request(default_tg, n, "en", "").url,
               StartsWith("https://api.telegram.org/bot123:ABC/"));

    const auto slack = build_request(channel("slack"), n, "en", "");
    CHECK(nlohmann::json::parse(slack.body)["text"] == n.text);

    auto hook_cfg = channel("webhook");
    hook_cfg.headers["Authorization"] = "Bearer t";
    const auto hook = build_request(hook_cfg, n, "en", "https://dash");
    CHECK(hook.headers.at("Authorization") == "Bearer t");
    const auto body = nlohmann::json::parse(hook.body);
    CHECK(body["type"] == "alert");
    CHECK(body["severity"] == "critical");
    CHECK(body["sensor_id"] == "winding_temp");
    CHECK(body["device_id"] == "transformer_1");
    CHECK(body["alert"]["kind"] == "limit");
    CHECK(body["alert"]["limit"] == 90.0);
    CHECK(body["alert"]["direction"] == "high");
    CHECK(body["dashboard_url"] == "https://dash");
}

namespace {

/// Records what the notifier sends; answers with a scripted status list.
struct FakeTransport {
    std::mutex mutex;
    std::vector<OutgoingRequest> sent;
    std::vector<int> statuses;  // consumed front to back; 200 once empty

    Notifier::Transport fn() {
        return [this](const OutgoingRequest& r) {
            std::lock_guard lock(mutex);
            sent.push_back(r);
            if (statuses.empty()) {
                return 200;
            }
            const int s = statuses.front();
            statuses.erase(statuses.begin());
            return s;
        };
    }
    std::size_t count() {
        std::lock_guard lock(mutex);
        return sent.size();
    }
};

NotificationsConfig webhook_config(Severity min_severity = Severity::warning) {
    NotificationsConfig cfg;
    cfg.language = "en";
    cfg.min_severity = min_severity;
    cfg.channels = {channel("webhook")};
    return cfg;
}

bool wait_until(const std::function<bool()>& predicate) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
    while (std::chrono::steady_clock::now() < deadline) {
        if (predicate()) {
            return true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    return false;
}

}  // namespace

TEST_CASE("Notifier retries server errors and reports the final result", "[notify][notifier]") {
    FakeTransport transport;
    transport.statuses = {503};  // first attempt fails, the retry succeeds
    std::atomic<int> ok{0};
    std::atomic<int> failed{0};
    Notifier notifier(
        webhook_config(),
        {{"winding_temp", kWinding}},
        [&](const std::string&, bool success) { (success ? ok : failed)++; },
        transport.fn());
    notifier.start();

    notifier.on_alert(limit_alert());

    REQUIRE(wait_until([&] { return ok.load() == 1; }));
    notifier.stop();
    CHECK(transport.count() == 2);
    CHECK(failed.load() == 0);
}

TEST_CASE("Notifier does not retry client errors such as a wrong token", "[notify][notifier]") {
    FakeTransport transport;
    transport.statuses = {401};
    std::atomic<int> failed{0};
    Notifier notifier(
        webhook_config(),
        {},
        [&](const std::string&, bool success) {
            if (!success) {
                ++failed;
            }
        },
        transport.fn());
    notifier.start();

    notifier.on_alert(limit_alert());

    REQUIRE(wait_until([&] { return failed.load() == 1; }));
    notifier.stop();
    CHECK(transport.count() == 1);
}

TEST_CASE("Notifier respects min_severity and only reports recoveries after an outage",
          "[notify][notifier]") {
    FakeTransport transport;
    Notifier notifier(webhook_config(Severity::critical), {}, {}, transport.fn());
    notifier.start();

    notifier.on_alert(statistical_alert());  // warning: filtered out
    const auto now = std::chrono::system_clock::now();
    notifier.on_device_status("plc", true, now);   // initial "online": not news
    notifier.on_device_status("plc", false, now);  // outage: sent
    notifier.on_device_status("plc", false, now);  // repeated: ignored
    notifier.on_device_status("plc", true, now);   // recovery: sent despite min_severity

    REQUIRE(wait_until([&] { return transport.count() == 2; }));
    notifier.stop();

    const auto first = nlohmann::json::parse(transport.sent[0].body);
    const auto second = nlohmann::json::parse(transport.sent[1].body);
    CHECK(first["type"] == "device_offline");
    CHECK(second["type"] == "device_online");
}

TEST_CASE("Notifier delivers what is queued before stopping", "[notify][notifier]") {
    FakeTransport transport;
    Notifier notifier(webhook_config(), {}, {}, transport.fn());
    notifier.start();
    for (int i = 0; i < 5; ++i) {
        notifier.on_alert(limit_alert());
    }
    notifier.stop(std::chrono::seconds(5));
    CHECK(transport.count() == 5);
}
