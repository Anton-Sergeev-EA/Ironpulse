# Header-only: detectors (z-score, EWMA, CUSUM), the rule engine and the
# factory that builds them from configuration.
add_library(ironpulse_analytics INTERFACE)
add_library(ironpulse::analytics ALIAS ironpulse_analytics)

target_include_directories(ironpulse_analytics
    INTERFACE
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
)

target_link_libraries(ironpulse_analytics INTERFACE ironpulse_options ironpulse::core)
