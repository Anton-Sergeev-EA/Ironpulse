#include "ironpulse/core/logger.hpp"

#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

namespace ironpulse::core {

std::shared_ptr<spdlog::logger> Logger::instance_ = nullptr;

namespace {
spdlog::level::level_enum to_spdlog_level(LogLevel level) {
    switch (level) {
        case LogLevel::trace:
            return spdlog::level::trace;
        case LogLevel::debug:
            return spdlog::level::debug;
        case LogLevel::info:
            return spdlog::level::info;
        case LogLevel::warn:
            return spdlog::level::warn;
        case LogLevel::error:
            return spdlog::level::err;
        case LogLevel::critical:
            return spdlog::level::critical;
    }
    return spdlog::level::info;
}
}  // namespace

void Logger::init(const std::string& app_name, LogLevel level, const std::string& log_file) {
    std::vector<spdlog::sink_ptr> sinks;
    sinks.push_back(std::make_shared<spdlog::sinks::stdout_color_sink_mt>());
    if (!log_file.empty()) {
        sinks.push_back(std::make_shared<spdlog::sinks::basic_file_sink_mt>(log_file, true));
    }

    instance_ = std::make_shared<spdlog::logger>(app_name, sinks.begin(), sinks.end());
    instance_->set_level(to_spdlog_level(level));
    instance_->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%n] [%^%l%$] %v");
    instance_->flush_on(spdlog::level::warn);
    spdlog::register_logger(instance_);
}

std::shared_ptr<spdlog::logger>& Logger::get() {
    if (!instance_) {
        init("ironpulse");
    }
    return instance_;
}

}  // namespace ironpulse::core
