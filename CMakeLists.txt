cmake_minimum_required(VERSION 3.15)
project(HighPerformanceServer VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Build options
option(BUILD_TESTS "Build tests" ON)
option(BUILD_BENCHMARKS "Build benchmarks" ON)

# Compiler flags
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    add_compile_options(-Wall -Wextra -Wpedantic -O3 -march=native)
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        add_compile_options(-g -O0 -fsanitize=address -fsanitize=undefined)
    endif()
endif()

# Include directories
include_directories(${CMAKE_SOURCE_DIR}/include)

# Source files
set(SERVER_SOURCES
    src/server.cpp
    src/event_loop.cpp
    src/thread_pool.cpp
    src/http_parser.cpp
    src/http_response.cpp
    src/router.cpp
    src/logger.cpp
    src/config.cpp
    src/connection.cpp
    src/metrics_collector.cpp
    src/metrics_storage.cpp
    src/websocket.cpp
    src/alert_manager.cpp
)

# Create library
add_library(server_lib STATIC ${SERVER_SOURCES})

# Main executable
add_executable(server
    src/main.cpp
)

target_link_libraries(server server_lib pthread)

# Tests
if(BUILD_TESTS)
    enable_testing()
    find_package(GTest QUIET)
    if(NOT GTest_FOUND)
        include(FetchContent)
        FetchContent_Declare(
            googletest
            GIT_REPOSITORY https://github.com/google/googletest.git
            GIT_TAG v1.14.0
        )
        FetchContent_MakeAvailable(googletest)
    endif()

    add_executable(tests
        tests/test_http_parser.cpp
        tests/test_http_response.cpp
        tests/test_router.cpp
        tests/test_thread_pool.cpp
        tests/test_server.cpp
    )

    target_link_libraries(tests server_lib GTest::gtest GTest::gtest_main pthread)
    add_test(NAME AllTests COMMAND tests)
endif()

# Benchmarks
if(BUILD_BENCHMARKS)
    add_executable(benchmarks
        benchmarks/benchmark_server.cpp
    )
    target_link_libraries(benchmarks server_lib pthread)
endif()

# Installation
install(TARGETS server DESTINATION bin)
install(DIRECTORY include/ DESTINATION include)
