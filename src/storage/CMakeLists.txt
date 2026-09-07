# Currently header-only (ring_buffer.hpp, series_store.hpp).
# Will gain a compiled component once wal_writer.cpp (persistence) lands —
# see docs/adr/0002-storage-format.md.
add_library(ironpulse_storage INTERFACE)
add_library(ironpulse::storage ALIAS ironpulse_storage)

target_include_directories(ironpulse_storage
    INTERFACE
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
)

target_link_libraries(ironpulse_storage INTERFACE ironpulse_options)
