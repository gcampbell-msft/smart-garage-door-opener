# Standalone test project - uses native compiler (not cross-compile)
cmake_minimum_required(VERSION 3.10)
project(garage_tests C CXX)

# Enable testing
enable_testing()

# Force Google Test to use the same runtime library as the project
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)

include(FetchContent)
FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
)
FetchContent_MakeAvailable(googletest)

# Include the main source directory for headers
include_directories(${CMAKE_SOURCE_DIR}/../main)
include_directories(${CMAKE_SOURCE_DIR}/../main/interfaces)

add_executable(tests 
    test_state_machine.cpp
    test_wifi_retry.cpp
    test_mqtt_retry.cpp
    ${CMAKE_SOURCE_DIR}/../main/garage_state_machine.c
    ${CMAKE_SOURCE_DIR}/../main/wifi_retry_manager.c
    ${CMAKE_SOURCE_DIR}/../main/mqtt_retry_manager.c
)
target_link_libraries(tests GTest::gtest_main)

# Set C standard
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

include(GoogleTest)
gtest_discover_tests(tests)

