# Standalone test project - uses native compiler (not cross-compile)
cmake_minimum_required(VERSION 3.10)
project(garage_tests C)

# Enable testing
enable_testing()

# Set C standard
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Include the main source directory for headers
include_directories(${CMAKE_SOURCE_DIR}/../main)

# State machine library (pure C, no hardware dependencies)
add_library(garage_state_machine STATIC
    ${CMAKE_SOURCE_DIR}/../main/garage_state_machine.c
)

# State machine tests executable
add_executable(test_state_machine
    test_state_machine.c
)

target_link_libraries(test_state_machine
    garage_state_machine
)

# Register test with CTest
add_test(NAME StateMachineTests COMMAND test_state_machine)

# Add custom target to run tests
add_custom_target(run_tests
    COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure
    DEPENDS test_state_machine
    COMMENT "Running tests..."
)

