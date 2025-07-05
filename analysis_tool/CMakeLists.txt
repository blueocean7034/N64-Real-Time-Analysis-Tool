cmake_minimum_required(VERSION 3.16)
project(analysis_tool LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

find_package(SDL2 REQUIRED)

add_library(analysis_tool SHARED src/AnalysisWindow.cpp)

target_include_directories(analysis_tool PUBLIC include ${SDL2_INCLUDE_DIRS} ../sky96/source/mupen64plus-core/src/api)

target_link_libraries(analysis_tool PRIVATE SDL2::SDL2)

add_executable(dummy_test tests/dummy_test.cpp)
target_include_directories(dummy_test PRIVATE include ../sky96/source/mupen64plus-core/src/api)
target_link_libraries(dummy_test PRIVATE analysis_tool)

enable_testing()
add_test(NAME dummy_test COMMAND dummy_test)
