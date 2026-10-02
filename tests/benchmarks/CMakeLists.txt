add_executable(ironpulse_benchmarks
    bench_ring_buffer.cpp
    bench_frame_codec.cpp
    bench_ingest.cpp
)

target_link_libraries(ironpulse_benchmarks
    PRIVATE
        ironpulse_options
        ironpulse::storage
        ironpulse::ingest
        ironpulse::protocol
        benchmark::benchmark_main
)
