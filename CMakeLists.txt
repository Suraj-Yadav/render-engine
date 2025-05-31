cmake_minimum_required(VERSION 3.13 FATAL_ERROR)
project(main)

set(CMAKE_CXX_STANDARD 23)

add_subdirectory(third_party)

find_package(spdlog CONFIG REQUIRED)
find_package(cpptrace CONFIG REQUIRED)
find_package(tinyfiledialogs CONFIG REQUIRED)
find_package(Stb REQUIRED)

add_library(
  engine STATIC ./src/shader.cpp ./src/camera.cpp ./src/loader.cpp
                ./src/env.cpp
)
target_include_directories(engine PUBLIC include ${Stb_INCLUDE_DIR})
target_link_libraries(engine PUBLIC magnum spdlog::spdlog cpptrace::cpptrace)
target_compile_options(engine PUBLIC -fsanitize=address,undefined)
target_link_options(engine PUBLIC -fsanitize=address,undefined -static-libasan)

add_executable(viewer ./src/viewer.cpp)
target_link_libraries(viewer PRIVATE engine tinyfiledialogs::tinyfiledialogs)
