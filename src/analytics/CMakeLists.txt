# Header-only for now (detector.hpp with ZScoreDetector).
# EWMA/CUSUM strategies and rule_engine.cpp are planned follow-ups.
add_library(ironpulse_analytics INTERFACE)
add_library(ironpulse::analytics ALIAS ironpulse_analytics)

target_include_directories(ironpulse_analytics
    INTERFACE
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
)

target_link_libraries(ironpulse_analytics INTERFACE ironpulse_options)
