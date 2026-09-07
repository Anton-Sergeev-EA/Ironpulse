add_library(ironpulse_core
    logger.cpp
    config.cpp
)
add_library(ironpulse::core ALIAS ironpulse_core)

target_include_directories(ironpulse_core
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
)

target_link_libraries(ironpulse_core
    PUBLIC
        ironpulse_options
        spdlog::spdlog
        nlohmann_json::nlohmann_json
)
