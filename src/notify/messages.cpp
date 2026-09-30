#include "ironpulse/notify/messages.hpp"

#include <cstdio>
#include <ctime>
#include <map>
#include <string_view>

namespace ironpulse::notify {

namespace {

struct Strings {
    const char* critical;
    const char* warning;
    const char* ok;
    const char* limit_high;
    const char* limit_low;
    const char* statistical;
    const char* device;
    const char* time;
    const char* dashboard;
    const char* offline;
    const char* online;
};

// Same languages and order as the dashboard (web/js/i18n/i18n.js).
const std::map<std::string, Strings, std::less<>>& catalog() {
    static const std::map<std::string, Strings, std::less<>> kCatalog{
        {"ru",
         {"КРИТИЧНО",
          "ПРЕДУПРЕЖДЕНИЕ",
          "НОРМА",
          "{name}: значение {value} выше предела {limit}",
          "{name}: значение {value} ниже предела {limit}",
          "{name}: необычное поведение — подтвердили {votes} из {total} детекторов, текущее значение {value}",
          "Устройство",
          "Время",
          "Дашборд",
          "Устройство {device} не отвечает",
          "Устройство {device} снова на связи"}},
        {"en",
         {"CRITICAL",
          "WARNING",
          "OK",
          "{name}: value {value} is above the limit {limit}",
          "{name}: value {value} is below the limit {limit}",
          "{name}: unusual behaviour — confirmed by {votes} of {total} detectors, current value {value}",
          "Device",
          "Time",
          "Dashboard",
          "Device {device} is not responding",
          "Device {device} is back online"}},
        {"zh",
         {"严重",
          "警告",
          "正常",
          "{name}：数值 {value} 高于上限 {limit}",
          "{name}：数值 {value} 低于下限 {limit}",
          "{name}：行为异常——{total} 个检测器中有 {votes} 个确认，当前值 {value}",
          "设备",
          "时间",
          "仪表板",
          "设备 {device} 无响应",
          "设备 {device} 已恢复连接"}},
        {"hi",
         {"गंभीर",
          "चेतावनी",
          "सामान्य",
          "{name}: मान {value} सीमा {limit} से ऊपर है",
          "{name}: मान {value} सीमा {limit} से नीचे है",
          "{name}: असामान्य व्यवहार — {total} में से {votes} डिटेक्टरों ने पुष्टि की, वर्तमान मान {value}",
          "डिवाइस",
          "समय",
          "डैशबोर्ड",
          "डिवाइस {device} जवाब नहीं दे रहा है",
          "डिवाइस {device} फिर से ऑनलाइन है"}},
        {"es",
         {"CRÍTICO",
          "ADVERTENCIA",
          "OK",
          "{name}: el valor {value} supera el límite {limit}",
          "{name}: el valor {value} está por debajo del límite {limit}",
          "{name}: comportamiento inusual — confirmado por {votes} de {total} detectores, valor actual "
          "{value}",
          "Dispositivo",
          "Hora",
          "Panel",
          "El dispositivo {device} no responde",
          "El dispositivo {device} vuelve a estar en línea"}},
        {"fr",
         {"CRITIQUE",
          "AVERTISSEMENT",
          "OK",
          "{name} : la valeur {value} dépasse la limite {limit}",
          "{name} : la valeur {value} est inférieure à la limite {limit}",
          "{name} : comportement inhabituel — confirmé par {votes} détecteurs sur {total}, valeur actuelle "
          "{value}",
          "Équipement",
          "Heure",
          "Tableau de bord",
          "L’équipement {device} ne répond pas",
          "L’équipement {device} est de nouveau en ligne"}},
        {"de",
         {"KRITISCH",
          "WARNUNG",
          "OK",
          "{name}: Wert {value} liegt über dem Grenzwert {limit}",
          "{name}: Wert {value} liegt unter dem Grenzwert {limit}",
          "{name}: ungewöhnliches Verhalten — von {votes} von {total} Detektoren bestätigt, aktueller Wert "
          "{value}",
          "Gerät",
          "Zeit",
          "Dashboard",
          "Gerät {device} antwortet nicht",
          "Gerät {device} ist wieder online"}},
        {"it",
         {"CRITICO",
          "AVVISO",
          "OK",
          "{name}: il valore {value} supera il limite {limit}",
          "{name}: il valore {value} è sotto il limite {limit}",
          "{name}: comportamento anomalo — confermato da {votes} rilevatori su {total}, valore attuale "
          "{value}",
          "Dispositivo",
          "Ora",
          "Dashboard",
          "Il dispositivo {device} non risponde",
          "Il dispositivo {device} è di nuovo online"}},
    };
    return kCatalog;
}

const Strings& strings_for(const std::string& language) {
    const auto& all = catalog();
    auto it = all.find(language);
    return it != all.end() ? it->second : all.at("ru");
}

std::string fill(std::string_view tmpl, const std::map<std::string, std::string, std::less<>>& params) {
    std::string out;
    out.reserve(tmpl.size() + 32);
    std::size_t pos = 0;
    while (pos < tmpl.size()) {
        const auto open = tmpl.find('{', pos);
        if (open == std::string_view::npos) {
            out.append(tmpl.substr(pos));
            break;
        }
        const auto close = tmpl.find('}', open);
        if (close == std::string_view::npos) {
            out.append(tmpl.substr(pos));
            break;
        }
        out.append(tmpl.substr(pos, open - pos));
        const auto key = tmpl.substr(open + 1, close - open - 1);
        auto it = params.find(key);
        out += it != params.end() ? it->second : std::string(tmpl.substr(open, close - open + 1));
        pos = close + 1;
    }
    return out;
}

std::string format_time(std::chrono::system_clock::time_point tp) {
    const std::time_t t = std::chrono::system_clock::to_time_t(tp);
    std::tm tm{};
#if defined(_WIN32)
    gmtime_s(&tm, &t);
#else
    gmtime_r(&t, &tm);
#endif
    char buffer[32];
    std::strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S UTC", &tm);
    return buffer;
}

std::string with_unit(double value, const std::string& unit) {
    return unit.empty() ? format_value(value) : format_value(value) + " " + unit;
}

const char* icon(core::Severity severity) {
    return severity == core::Severity::critical ? "\xF0\x9F\x94\xB4" : "\xF0\x9F\x9F\xA0";  // 🔴 / 🟠
}

std::string footer(const Strings& s,
                   const std::string& device_id,
                   std::chrono::system_clock::time_point timestamp,
                   const std::string& dashboard_url) {
    std::string out;
    if (!device_id.empty()) {
        out += std::string("\n") + s.device + ": " + device_id;
    }
    out += std::string("\n") + s.time + ": " + format_time(timestamp);
    if (!dashboard_url.empty()) {
        out += std::string("\n") + s.dashboard + ": " + dashboard_url;
    }
    return out;
}

}  // namespace

std::string format_value(double value) {
    char buffer[64];
    std::snprintf(buffer, sizeof(buffer), "%.2f", value);
    std::string s(buffer);
    if (s.find('.') != std::string::npos) {
        while (!s.empty() && s.back() == '0') {
            s.pop_back();
        }
        if (!s.empty() && s.back() == '.') {
            s.pop_back();
        }
    }
    return s == "-0" ? "0" : s;
}

Notification describe_alert(const core::AnomalyEvent& alert,
                            const SensorInfo& sensor,
                            const std::string& language,
                            const std::string& dashboard_url) {
    const Strings& s = strings_for(language);
    const std::string name = sensor.name.empty() ? alert.sensor_id : sensor.name;

    std::string line;
    if (alert.kind == core::AlertKind::limit && alert.limit) {
        line = fill(alert.above_limit ? s.limit_high : s.limit_low,
                    {{"name", name},
                     {"value", with_unit(alert.value, sensor.unit)},
                     {"limit", with_unit(*alert.limit, sensor.unit)}});
    } else {
        line = fill(s.statistical,
                    {{"name", name},
                     {"value", with_unit(alert.value, sensor.unit)},
                     {"votes", std::to_string(alert.votes)},
                     {"total", std::to_string(alert.detectors_total)}});
    }

    Notification n;
    n.type = Notification::Type::alert;
    n.severity = alert.severity;
    n.timestamp = alert.timestamp;
    n.sensor_id = alert.sensor_id;
    n.device_id = sensor.device_id;
    n.alert = alert;
    n.title = std::string(alert.severity == core::Severity::critical ? s.critical : s.warning) + ": " + line;
    n.text = std::string(icon(alert.severity)) + " " +
             (alert.severity == core::Severity::critical ? s.critical : s.warning) + "\n" + line +
             footer(s, sensor.device_id, alert.timestamp, dashboard_url);
    return n;
}

Notification describe_device_status(const std::string& device_id,
                                    bool online,
                                    std::chrono::system_clock::time_point timestamp,
                                    const std::string& language,
                                    const std::string& dashboard_url) {
    const Strings& s = strings_for(language);
    const std::string line = fill(online ? s.online : s.offline, {{"device", device_id}});

    Notification n;
    n.type = online ? Notification::Type::device_online : Notification::Type::device_offline;
    n.severity = online ? core::Severity::warning : core::Severity::critical;
    n.timestamp = timestamp;
    n.device_id = device_id;
    const char* label = online ? s.ok : s.critical;
    n.title = std::string(label) + ": " + line;
    n.text = std::string(online ? "\xF0\x9F\x9F\xA2" : icon(core::Severity::critical)) + " " + label + "\n" +
             line + footer(s, {}, timestamp, dashboard_url);
    return n;
}

}  // namespace ironpulse::notify
