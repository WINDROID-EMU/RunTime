# rexcore - Core utilities library
# Foundation types, data structures, and threading primitives (ARM64 Android)

add_library(rexcore OBJECT
    arena.cpp
    bit_map.cpp
    bit_stream.cpp
    byte_stream.cpp
    clock.cpp
    cvar.cpp
    exception_handler.cpp
    filesystem.cpp
    filesystem_wildcard.cpp
    hash.cpp
    logging.cpp
    memory.cpp
    mutex.cpp
    ring_buffer.cpp
    string_buffer.cpp
    threading.cpp
    timer_queue.cpp
    utf8.cpp
    vec128.cpp

    # POSIX / Android platform sources
    atomic_posix.cpp
    clock_posix.cpp
    dbg_posix.cpp
    dynlib_posix.cpp
    exception_handler_posix.cpp
    filesystem_posix.cpp
    fiber_posix.cpp
    mapped_memory_posix.cpp
    math_gcc.cpp
    memory_posix.cpp
    platform/console_posix.cpp
    platform/env_posix.cpp
    seh_posix.cpp
    socket_posix.cpp
    system_posix.cpp
    threading_posix.cpp
)
add_library(rex::core ALIAS rexcore)

# Xenos data-format layer (stateless; headers under include/rex/graphics/).
# The GPU emulation consuming it lives in the rexgpu-xenos plugin.
target_sources(rexcore PRIVATE
    ${PROJECT_SOURCE_DIR}/src/graphics/xenos.cpp
    ${PROJECT_SOURCE_DIR}/src/graphics/pipeline/texture/info.cpp
    ${PROJECT_SOURCE_DIR}/src/graphics/pipeline/texture/info_formats.cpp
    ${PROJECT_SOURCE_DIR}/src/graphics/pipeline/texture/extent.cpp
    ${PROJECT_SOURCE_DIR}/src/graphics/pipeline/texture/util.cpp
    ${PROJECT_SOURCE_DIR}/src/graphics/pipeline/texture/conversion.cpp
)

target_include_directories(rexcore PUBLIC
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
    $<BUILD_INTERFACE:${PROJECT_BINARY_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

target_include_directories(rexcore PRIVATE
    ${PROJECT_SOURCE_DIR}
    ${PROJECT_SOURCE_DIR}/thirdparty/cli11/include
)

target_link_libraries(rexcore PRIVATE log dl)

target_link_libraries(rexcore
    PUBLIC
        fmt::fmt
        simde
        tomlplusplus
        spdlog::spdlog
    PRIVATE
        disruptorplus
        utf8cpp
        xxHash::xxhash
)

target_compile_definitions(rexcore PUBLIC REX_HAS_VULKAN=1)

if(REXGLUE_ENABLE_TRACY)
    target_link_libraries(rexcore PUBLIC
        $<$<NOT:$<CONFIG:Release>>:$<TARGET_NAME:TracyClient>>)
    target_compile_definitions(rexcore PUBLIC
        $<$<NOT:$<CONFIG:Release>>:REXGLUE_ENABLE_PROFILING>)
endif()

if(REXGLUE_ENABLE_PERF_COUNTERS)
    target_sources(rexcore PRIVATE perf/counter.cpp)
    target_compile_definitions(rexcore PUBLIC
        $<$<NOT:$<CONFIG:Release>>:REXGLUE_ENABLE_PERF_COUNTERS>)
endif()
