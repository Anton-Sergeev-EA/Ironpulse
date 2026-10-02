# Telemetry export: lock-free SPSC queue, CRC-checked batch encoding and
# segment files (originally the apollonian_core_ingestor project).
add_library(ironpulse_ingest
    ingestor.cpp
    segment.cpp
    reading_exporter.cpp
)
add_library(ironpulse::ingest ALIAS ironpulse_ingest)

target_include_directories(ironpulse_ingest
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
)

target_link_libraries(ironpulse_ingest
    PUBLIC
        ironpulse_options
        ironpulse::core
)

# Command-line reader for export segments: verify, list tags, dump to CSV.
add_executable(ironpulse-export export_tool.cpp)
target_link_libraries(ironpulse-export PRIVATE ironpulse_options ironpulse::ingest)
install(TARGETS ironpulse-export RUNTIME DESTINATION bin)
