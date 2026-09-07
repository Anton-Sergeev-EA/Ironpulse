add_library(ironpulse_protocol
    frame_codec.cpp
    modbus_client.cpp
)
add_library(ironpulse::protocol ALIAS ironpulse_protocol)

target_include_directories(ironpulse_protocol
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
)

target_link_libraries(ironpulse_protocol
    PUBLIC
        ironpulse_options
        ironpulse::core
        asio
)
