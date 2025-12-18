cmake_minimum_required(VERSION 3.15.0)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")

project(controller-test-task VERSION 0.1.0 LANGUAGES C CXX)

# External libraries
include(FetchContent)
set(FETCHCONTENT_QUIET OFF)
FetchContent_Declare(
    gtest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.17.0
)
FetchContent_MakeAvailable(gtest)
FetchContent_Declare(
    spdlog
    GIT_REPOSITORY https://github.com/gabime/spdlog.git
    GIT_TAG v1.16.0
)
FetchContent_MakeAvailable(spdlog)


# Executable
add_library(
  run_lib
  STATIC
  src/virtual_serial.cpp
  src/Idevice.cpp
  src/device_a.cpp
  src/device_b.cpp
  src/data_packet.cpp
)
target_link_libraries(
  run_lib
  PRIVATE
  spdlog
)
target_include_directories(
  run_lib
  PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/src
)


add_executable(
  controller-test-task 
  src/main.cpp
)
target_link_libraries(
  controller-test-task
  PUBLIC
  run_lib
)


# Tests
enable_testing()
add_executable(
  Tests 
  test/tests.cpp
)
target_link_libraries(
  Tests
  PUBLIC
  GTest::gtest_main
  run_lib
)
include(GoogleTest)
gtest_discover_tests(Tests)
