add_library(ironpulse_notify
    messages.cpp
    notifier.cpp
)
add_library(ironpulse::notify ALIAS ironpulse_notify)

target_include_directories(ironpulse_notify
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
)

# httplib::httplib carries OpenSSL (and CPPHTTPLIB_OPENSSL_SUPPORT) when
# OpenSSL >= 3 is found at configure time — required for https:// targets
# such as Telegram and Slack.
target_link_libraries(ironpulse_notify
    PUBLIC
        ironpulse_options
        ironpulse::core
        httplib::httplib
        nlohmann_json::nlohmann_json
)
