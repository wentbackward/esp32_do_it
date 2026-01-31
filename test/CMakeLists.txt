cmake_minimum_required(VERSION 3.14)
project(trackpad_tests C)

# Compiler flags
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -g")

# Fetch Unity test framework automatically
include(FetchContent)

FetchContent_Declare(
    unity
    GIT_REPOSITORY https://github.com/ThrowTheSwitch/Unity.git
    GIT_TAG v2.5.2
    GIT_SHALLOW TRUE
)

# Make Unity available
FetchContent_MakeAvailable(unity)

# Include directories
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../main
    ${unity_SOURCE_DIR}/src
)

# Source files
set(TRACKPAD_SOURCES
    ../main/trackpad_gesture.c
)

set(TEST_HELPER_SOURCES
    trackpad_test_helper.c
)

set(UNITY_SOURCES
    ${unity_SOURCE_DIR}/src/unity.c
)

set(TEST_SOURCES
    test_trackpad_gesture.c
)

# Executable
add_executable(trackpad_tests
    ${TRACKPAD_SOURCES}
    ${TEST_HELPER_SOURCES}
    ${UNITY_SOURCES}
    ${TEST_SOURCES}
)

# Link math library
target_link_libraries(trackpad_tests m)

# Enable testing
enable_testing()
add_test(NAME trackpad_tests COMMAND trackpad_tests)

# Print instructions
message(STATUS "")
message(STATUS "================================================")
message(STATUS "Trackpad Tests Build Configuration")
message(STATUS "================================================")
message(STATUS "  Unity will be downloaded automatically")
message(STATUS "  Build tests: cmake .. && make")
message(STATUS "  Run tests:   ./trackpad_tests")
message(STATUS "================================================")
message(STATUS "")
