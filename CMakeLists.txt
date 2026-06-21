cmake_minimum_required(VERSION 3.13 FATAL_ERROR)
project(main)

set(CMAKE_CXX_STANDARD 23)

include(cmake/Engine.cmake)

add_executable(viewer ./src/viewer.cpp)
target_link_libraries(viewer PRIVATE engine)
