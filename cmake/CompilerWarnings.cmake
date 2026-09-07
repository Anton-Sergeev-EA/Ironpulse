# Sets a strict, sane set of compiler warnings on the given interface target.
function(ironpulse_set_project_warnings target_name warnings_as_errors)
    set(CLANG_GCC_WARNINGS
        -Wall
        -Wextra
        -Wpedantic
        -Wshadow
        -Wnon-virtual-dtor
        -Wold-style-cast
        -Wcast-align
        -Wunused
        -Woverloaded-virtual
        -Wconversion
        -Wsign-conversion
        -Wdouble-promotion
        -Wformat=2
        -Wimplicit-fallthrough
    )

    if(warnings_as_errors)
        list(APPEND CLANG_GCC_WARNINGS -Werror)
    endif()

    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        # -Wnull-dereference is kept for Clang, which doesn't share GCC's
        # false-positive interaction with std::function/Asio below.
        target_compile_options(${target_name} INTERFACE ${CLANG_GCC_WARNINGS} -Wnull-dereference)
    elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        # -Wnull-dereference is deliberately omitted for GCC: at -O2/-O3 it
        # produces false positives when std::function comparisons from
        # Asio's io_context get inlined into our translation units (a
        # known GCC limitation, not a bug in this codebase or in Asio —
        # see gcc.gnu.org/bugzilla PR90493 and similar reports). Every
        # other warning below stays enabled.
        target_compile_options(${target_name} INTERFACE ${CLANG_GCC_WARNINGS} -Wduplicated-cond -Wduplicated-branches -Wlogical-op)
    elseif(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
        target_compile_options(${target_name} INTERFACE /W4 $<$<BOOL:${warnings_as_errors}>:/WX>)
    else()
        message(AUTHOR_WARNING "No compiler warnings set for '${CMAKE_CXX_COMPILER_ID}' compiler.")
    endif()
endfunction()
