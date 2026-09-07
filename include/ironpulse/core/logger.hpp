#pragma once

#include <memory>
#include <string>

// The IP_LOG_* macros below expand to spdlog's own SPDLOG_LOGGER_* macros,
// which requires the full spdlog header (macro definitions, not just a
// forward declaration of spdlog::logger).
#include <spdlog/spdlog.h>

namespace ironpulse::core {

enum class LogLevel { trace, debug, info, warn, error, critical };

/// Thin wrapper around spdlog so the rest of the codebase never includes
/// spdlog headers directly — keeps the logging backend swappable.
class Logger {
public:
    static void init(const std::string& app_name,
                     LogLevel level = LogLevel::info,
                     const std::string& log_file = "");

    static std::shared_ptr<spdlog::logger>& get();

private:
    static std::shared_ptr<spdlog::logger> instance_;
};

}  // namespace ironpulse::core

#define IP_LOG_TRACE(...) SPDLOG_LOGGER_TRACE(::ironpulse::core::Logger::get(), __VA_ARGS__)
#define IP_LOG_DEBUG(...) SPDLOG_LOGGER_DEBUG(::ironpulse::core::Logger::get(), __VA_ARGS__)
#define IP_LOG_INFO(...) SPDLOG_LOGGER_INFO(::ironpulse::core::Logger::get(), __VA_ARGS__)
#define IP_LOG_WARN(...) SPDLOG_LOGGER_WARN(::ironpulse::core::Logger::get(), __VA_ARGS__)
#define IP_LOG_ERROR(...) SPDLOG_LOGGER_ERROR(::ironpulse::core::Logger::get(), __VA_ARGS__)
#define IP_LOG_CRITICAL(...) SPDLOG_LOGGER_CRITICAL(::ironpulse::core::Logger::get(), __VA_ARGS__)
