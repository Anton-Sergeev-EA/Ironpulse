add_library(ironpulse_api
    http_server.cpp
    ws_server.cpp
)
add_library(ironpulse::api ALIAS ironpulse_api)

target_include_directories(ironpulse_api
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
)

target_link_libraries(ironpulse_api
    PUBLIC
        ironpulse_options
        ironpulse::core
        ironpulse::storage
        asio
        httplib::httplib
        nlohmann_json::nlohmann_json
)
