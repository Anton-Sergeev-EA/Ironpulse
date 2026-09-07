cmake_minimum_required(VERSION 3.20)

project(ironpulse
    VERSION 0.1.0
    DESCRIPTION "Industrial telemetry & anomaly detection engine"
    LANGUAGES CXX
)

# ---------------------------------------------------------------------------
# Global settings
# ---------------------------------------------------------------------------
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE "Debug" CACHE STRING "Build type" FORCE)
endif()

list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")

option(IRONPULSE_BUILD_TESTS "Build unit/integration tests" ON)
option(IRONPULSE_BUILD_BENCHMARKS "Build benchmarks" OFF)
option(IRONPULSE_ENABLE_ASAN "Enable AddressSanitizer + UBSan" OFF)
option(IRONPULSE_ENABLE_TSAN "Enable ThreadSanitizer" OFF)
option(IRONPULSE_WARNINGS_AS_ERRORS "Treat warnings as errors" OFF)

include(CompilerWarnings)
include(Sanitizers)
include(Dependencies)

# ---------------------------------------------------------------------------
# Interface target: shared compile options for the whole project
# ---------------------------------------------------------------------------
add_library(ironpulse_options INTERFACE)
target_compile_features(ironpulse_options INTERFACE cxx_std_20)
ironpulse_set_project_warnings(ironpulse_options ${IRONPULSE_WARNINGS_AS_ERRORS})
ironpulse_enable_sanitizers(ironpulse_options)

# ---------------------------------------------------------------------------
# Subdirectories
# ---------------------------------------------------------------------------
add_subdirectory(src/core)
add_subdirectory(src/protocol)
add_subdirectory(src/storage)
add_subdirectory(src/analytics)
add_subdirectory(src/api)

add_executable(ironpulse src/main.cpp)
target_link_libraries(ironpulse
    PRIVATE
        ironpulse_options
        ironpulse::core
        ironpulse::protocol
        ironpulse::storage
        ironpulse::analytics
        ironpulse::api
)

if(IRONPULSE_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

if(IRONPULSE_BUILD_BENCHMARKS)
    if(EXISTS "${CMAKE_SOURCE_DIR}/tests/benchmarks/CMakeLists.txt")
        add_subdirectory(tests/benchmarks)
    else()
        message(WARNING "IRONPULSE_BUILD_BENCHMARKS is ON but tests/benchmarks/CMakeLists.txt "
                         "does not exist — skipping. (This should not happen in a checked-out "
                         "copy of the repo; if you see this, the benchmarks directory is missing "
                         "or was not copied into the build context.)")
    endif()
endif()

install(TARGETS ironpulse RUNTIME DESTINATION bin)
