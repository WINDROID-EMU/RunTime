# ============================================================
# ReXGlue Android App — Native CMake Build
# ============================================================

cmake_minimum_required(VERSION 3.25)
project(rexglue_android_bridge LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 1. Core Runtime Shared Library (librexruntime.so)
add_library(rexruntime SHARED
    rexruntime.cpp
)

target_link_libraries(rexruntime PRIVATE
    android
    log
)

# 2. JNI Bridge Shared Library (librexglue_jni.so)
add_library(rexglue_jni SHARED
    jni_bridge.cpp
)

target_link_libraries(rexglue_jni PRIVATE
    rexruntime
    android
    log
)

set_target_properties(rexruntime rexglue_jni PROPERTIES
    CXX_VISIBILITY_PRESET default
)
