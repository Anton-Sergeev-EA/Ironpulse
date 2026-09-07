add_executable(ironpulse_tests
    unit/protocol/test_frame_codec.cpp
    unit/core/test_thread_pool.cpp
    unit/core/test_event_bus.cpp
    unit/core/test_config.cpp
    unit/storage/test_ring_buffer.cpp
    unit/storage/test_wal.cpp
    unit/analytics/test_detector.cpp
    unit/analytics/test_ewma.cpp
    unit/analytics/test_cusum.cpp
    unit/analytics/test_rule_engine.cpp
)

target_link_libraries(ironpulse_tests
    PRIVATE
        ironpulse_options
        ironpulse::core
        ironpulse::protocol
        ironpulse::storage
        ironpulse::analytics
        Catch2::Catch2WithMain
)

include(CTest)
include(Catch)
catch_discover_tests(ironpulse_tests)
