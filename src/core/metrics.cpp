#include "ironpulse/core/metrics.hpp"

#include <cmath>
#include <sstream>

namespace ironpulse::core {

void Metrics::describe(const std::string& name, Type type, const std::string& help) {
    std::lock_guard lock(mutex_);
    auto& family = families_[name];
    family.type = type;
    family.help = help;
    family.described = true;
}

void Metrics::increment(const std::string& name, const Labels& labels, double delta) {
    const std::string key = render_labels(labels);
    std::lock_guard lock(mutex_);
    families_[name].series[key] += delta;
}

void Metrics::set(const std::string& name, const Labels& labels, double value) {
    const std::string key = render_labels(labels);
    std::lock_guard lock(mutex_);
    families_[name].series[key] = value;
}

double Metrics::value(const std::string& name, const Labels& labels) const {
    const std::string key = render_labels(labels);
    std::lock_guard lock(mutex_);
    auto family = families_.find(name);
    if (family == families_.end()) {
        return 0.0;
    }
    auto series = family->second.series.find(key);
    return series == family->second.series.end() ? 0.0 : series->second;
}

std::string Metrics::escape_label_value(const std::string& value) {
    std::string out;
    out.reserve(value.size());
    for (const char c : value) {
        switch (c) {
            case '\\':
                out += "\\\\";
                break;
            case '"':
                out += "\\\"";
                break;
            case '\n':
                out += "\\n";
                break;
            default:
                out += c;
        }
    }
    return out;
}

std::string Metrics::render_labels(const Labels& labels) {
    if (labels.empty()) {
        return {};
    }
    std::string out = "{";
    for (std::size_t i = 0; i < labels.size(); ++i) {
        if (i > 0) {
            out += ',';
        }
        out += labels[i].first + "=\"" + escape_label_value(labels[i].second) + '"';
    }
    out += '}';
    return out;
}

std::string Metrics::render() const {
    std::ostringstream out;
    out.precision(17);
    std::lock_guard lock(mutex_);
    for (const auto& [name, family] : families_) {
        if (family.series.empty()) {
            continue;
        }
        if (family.described) {
            out << "# HELP " << name << ' ' << family.help << '\n';
            out << "# TYPE " << name << ' ' << (family.type == Type::counter ? "counter" : "gauge") << '\n';
        }
        for (const auto& [labels, value] : family.series) {
            out << name << labels << ' ';
            if (std::isnan(value)) {
                out << "NaN";
            } else if (std::isinf(value)) {
                out << (value > 0 ? "+Inf" : "-Inf");
            } else {
                out << value;
            }
            out << '\n';
        }
    }
    return out.str();
}

}  // namespace ironpulse::core
