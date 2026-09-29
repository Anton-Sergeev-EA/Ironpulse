#pragma once

#include <map>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

namespace ironpulse::core {

/// Thread-safe registry of counters and gauges, rendered in the
/// Prometheus text exposition format for GET /metrics.
///
/// Deliberately tiny instead of pulling in a client library: the engine
/// exports a few dozen series, and the format is simple and stable
/// (https://prometheus.io/docs/instrumenting/exposition_formats/).
class Metrics {
public:
    using Labels = std::vector<std::pair<std::string, std::string>>;

    enum class Type { counter, gauge };

    /// Declares a metric family's type and help text. Families used
    /// without being described are exported as untyped gauges.
    void describe(const std::string& name, Type type, const std::string& help);

    void increment(const std::string& name, const Labels& labels = {}, double delta = 1.0);
    void set(const std::string& name, const Labels& labels, double value);
    void set(const std::string& name, double value) {
        set(name, {}, value);
    }

    /// Current value of one series, or 0 if it was never written.
    [[nodiscard]] double value(const std::string& name, const Labels& labels = {}) const;

    [[nodiscard]] std::string render() const;

    /// Escapes a label value per the exposition format (\\, \" and \n).
    [[nodiscard]] static std::string escape_label_value(const std::string& value);

private:
    struct Family {
        Type type = Type::gauge;
        std::string help;
        bool described = false;
        std::map<std::string, double> series;  // rendered label set -> value
    };

    static std::string render_labels(const Labels& labels);

    mutable std::mutex mutex_;
    std::map<std::string, Family> families_;
};

}  // namespace ironpulse::core
