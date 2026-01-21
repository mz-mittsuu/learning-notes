cmake_minimum_required(VERSION 3.20)
project(alarm_ut LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(ENABLE_COVERAGE "Enable coverage flags" OFF) # Windowsでは通常OFF

add_library(alarm
  alarm/alarm.cpp
)
target_include_directories(alarm PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

include(FetchContent)
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

enable_testing()

add_executable(alarm_tests
  tests/test_alarm.cpp
)

target_link_libraries(alarm_tests
  alarm
  GTest::gtest_main
  GTest::gmock
)

include(GoogleTest)
gtest_discover_tests(alarm_tests)
