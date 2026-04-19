# Copyright (c)  Zubax Robotics  <zubax.com>
# Author: Pavel Kirienko <pavel.kirienko@zubax.com>

cmake_minimum_required(VERSION 3.20)
project(tolstoy CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# The library itself: header-only, usable via target_link_libraries(... tolstoy).
add_library(tolstoy INTERFACE)
add_library(tolstoy::tolstoy ALIAS tolstoy)
target_include_directories(tolstoy INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)
target_compile_features(tolstoy INTERFACE cxx_std_23)

# Tests are built by default when Tolstoy is the top-level project; disabled when consumed as a subdir.
if(NOT DEFINED PROJECT_IS_TOP_LEVEL)
    # CMake < 3.21 does not define PROJECT_IS_TOP_LEVEL; emulate it.
    if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
        set(PROJECT_IS_TOP_LEVEL TRUE)
    else()
        set(PROJECT_IS_TOP_LEVEL FALSE)
    endif()
endif()
option(TOLSTOY_BUILD_TESTS "Build tolstoy tests" ${PROJECT_IS_TOP_LEVEL})
if(TOLSTOY_BUILD_TESTS)
    enable_testing()
    add_subdirectory(test)
endif()
