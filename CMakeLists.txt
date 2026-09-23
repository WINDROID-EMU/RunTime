# ReXGlue SDK - Xbox 360 Recompilation Toolkit (ARM64 Android)
cmake_minimum_required(VERSION 3.25)

project(ReXGlue
    VERSION 0.10.0
    LANGUAGES C CXX
    DESCRIPTION "Xbox 360 Recompilation SDK (ARM64 Android)"
)

#==========================================================
# Build Options
#==========================================================
option(REXGLUE_BUILD_TESTS       "Build PPC instruction and unit tests" OFF)
option(REXGLUE_ENABLE_SANITIZERS "Enable UndefinedBehaviorSanitizer" OFF)
option(REXGLUE_ENABLE_FIDELITYFX "Enable optional AMD FidelityFX integration (experimental)" OFF)
option(REXGLUE_ENABLE_TRACY      "Enable Tracy profiler integration" OFF)
option(REXGLUE_ENABLE_PERF_COUNTERS "Enable lightweight perf counter registry and overlay" ON)

if(REXGLUE_ENABLE_TRACY)
    add_compile_definitions($<$<NOT:$<CONFIG:Release>>:REXGLUE_ENABLE_PROFILING>)
endif()
if(REXGLUE_ENABLE_PERF_COUNTERS)
    add_compile_definitions($<$<NOT:$<CONFIG:Release>>:REXGLUE_ENABLE_PERF_COUNTERS>)
endif()

# Graphics backend: Vulkan only
set(REXGLUE_USE_D3D12 OFF CACHE INTERNAL "D3D12 is disabled on Android")
set(REXGLUE_USE_VULKAN ON CACHE INTERNAL "Vulkan is the standard graphics backend for Android")

# Root of the SDK source tree
set(REXGLUE_ROOT "${CMAKE_CURRENT_SOURCE_DIR}")

# SDK install support
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/rexglue_helpers.cmake)

# Set C++23 standard
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_DEBUG_POSTFIX d)
set(CMAKE_RELWITHDEBINFO_POSTFIX rd)

function(rex_config_postfix target)
    set_target_properties(${target} PROPERTIES
        DEBUG_POSTFIX "${CMAKE_DEBUG_POSTFIX}"
        RELWITHDEBINFO_POSTFIX "${CMAKE_RELWITHDEBINFO_POSTFIX}")
endfunction()

# Code model and PIC for ARM64 Android
add_compile_options(-mcmodel=small)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Global compile options
add_compile_options(
    -fno-strict-aliasing
    -ffp-model=strict
    $<$<CONFIG:DEBUG>:-g>
    $<$<CONFIG:DEBUG>:-O0>
    $<$<CONFIG:RELEASE>:-O3>
    $<$<CONFIG:RELEASE>:-DNDEBUG>
    $<$<CONFIG:RELEASE>:-g0>
    $<$<CONFIG:RELEASE>:-fomit-frame-pointer>
    -fno-char8_t
)

# Global link options
add_link_options(
    $<$<CONFIG:RELEASE>:-Wl,--gc-sections>
)

# Platform and Architecture: Android ARM64
set(REX_PLATFORM "android-arm64")
set(REX_TARGET_PROCESSOR "aarch64")

add_compile_definitions(
    REX_PLATFORM_ANDROID=1
    REX_PLATFORM_LINUX=1
    REX_ARCH_ARM64=1
)

# Build config as compile definition
add_compile_definitions(REXGLUE_BUILD_CONFIG="$<CONFIG>")

include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/rex_pch.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/rex_version.cmake)
rex_resolve_version(REXGLUE_FULL_VERSION
    FLOOR_MAJOR ${PROJECT_VERSION_MAJOR}
    FLOOR_MINOR ${PROJECT_VERSION_MINOR})

string(REGEX REPLACE "-.*$" "" REXGLUE_NUMERIC_VERSION "${REXGLUE_FULL_VERSION}")
string(REGEX MATCH "^([0-9]+)\\.([0-9]+)(\\.([0-9]+))?(\\.([0-9]+))?" _ "${REXGLUE_NUMERIC_VERSION}")
set(REXGLUE_DERIVED_MAJOR "${CMAKE_MATCH_1}")
set(REXGLUE_DERIVED_MINOR "${CMAKE_MATCH_2}")
if(CMAKE_MATCH_4)
    set(REXGLUE_DERIVED_PATCH "${CMAKE_MATCH_4}")
else()
    set(REXGLUE_DERIVED_PATCH "0")
endif()
if(CMAKE_MATCH_6)
    set(REXGLUE_DERIVED_TWEAK "${CMAKE_MATCH_6}")
else()
    set(REXGLUE_DERIVED_TWEAK "0")
endif()

set(REXGLUE_FLOOR_VERSION
    "${REXGLUE_DERIVED_MAJOR}.${REXGLUE_DERIVED_MINOR}.${REXGLUE_DERIVED_PATCH}")

rex_version_channel(REXGLUE_VERSION_CHANNEL VERSION "${REXGLUE_FULL_VERSION}")

string(TIMESTAMP REXGLUE_BUILD_TIMESTAMP "%Y%m%d_%H%M")
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/include/rex/version.h.in
    ${CMAKE_CURRENT_BINARY_DIR}/include/rex/version.h
    @ONLY
)

# Output directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${REXGLUE_ROOT}/out/${REX_PLATFORM}")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${REXGLUE_ROOT}/out/${REX_PLATFORM}")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${REXGLUE_ROOT}/out/${REX_PLATFORM}")

if(REXGLUE_ENABLE_SANITIZERS)
    add_compile_options(-fsanitize=undefined -fno-omit-frame-pointer -g)
    add_link_options(-fsanitize=undefined)
endif()

if(REXGLUE_BUILD_TESTS)
    list(APPEND CMAKE_MODULE_PATH "${REXGLUE_ROOT}/cmake")
    list(APPEND CMAKE_MODULE_PATH "${REXGLUE_ROOT}/thirdparty/catch2/extras")
endif()

# Third-party dependencies
add_subdirectory(thirdparty)

add_compile_options(
    -Wall
    -Wextra
)

include_directories(${REXGLUE_ROOT}/include)

add_subdirectory(src/core)
add_subdirectory(src/filesystem)
add_subdirectory(src/ui)
add_subdirectory(src/input)
add_subdirectory(src/audio)
add_subdirectory(src/graphics)
add_subdirectory(src/system)
add_subdirectory(src/kernel)
add_subdirectory(src/codegen)
add_subdirectory(src/rexglue)

if(REXGLUE_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

include(cmake/rexglue_link_audit.cmake)

message(STATUS "===========================================")
message(STATUS "ReXGlue Configuration Summary:")
message(STATUS "  Version: ${REXGLUE_FULL_VERSION} (${REXGLUE_VERSION_CHANNEL})")
message(STATUS "  Platform: ${REX_PLATFORM}")
message(STATUS "  Compiler: ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "  Configurations: ${CMAKE_CONFIGURATION_TYPES}")
message(STATUS "  Output Dir: ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
message(STATUS "  C++ Standard: C++${CMAKE_CXX_STANDARD}")
message(STATUS "  Graphics: Vulkan (D3D12 disabled)")
message(STATUS "  FidelityFX: ${REXGLUE_ENABLE_FIDELITYFX}")
message(STATUS "  Tracy: ${REXGLUE_ENABLE_TRACY}")
message(STATUS "  Perf Counters: ${REXGLUE_ENABLE_PERF_COUNTERS}")
message(STATUS "===========================================")

if(PROJECT_IS_TOP_LEVEL)
    include(cmake/rexglue_devenv.cmake)
    include(cmake/rexglue_install.cmake)
else()
    set(REXGLUE_SHARE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src/ui" PARENT_SCOPE)
    set(REXGLUE_SHARE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src/ui")
    include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/rexglue_helpers.cmake)
endif()
