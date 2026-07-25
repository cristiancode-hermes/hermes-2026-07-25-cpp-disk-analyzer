cmake_minimum_required(VERSION 3.28)
project(tests LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Catch2 comes from parent FetchContent — use add_test directly
add_executable(test_suite test_main.cpp)
target_include_directories(test_suite PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(test_suite PRIVATE
  Catch2::Catch2WithMain
  diskwatch-lib
  spdlog::spdlog
  nlohmann_json::nlohmann_json
)

include(CTest)
add_test(NAME test_suite COMMAND test_suite)
