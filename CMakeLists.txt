cmake_minimum_required(VERSION 3.13 FATAL_ERROR)
project(main)

set(CMAKE_CXX_STANDARD 23)

find_package(spdlog CONFIG REQUIRED)
find_package(cpptrace CONFIG REQUIRED)
find_package(tinyfiledialogs CONFIG REQUIRED)

list(
  APPEND
  MAGNUM_FEATURES
  Magnum::AnyImageConverter
  Magnum::AnyImageImporter
  Magnum::AnySceneImporter
  Magnum::GlfwApplication
  Magnum::MeshTools
  Magnum::Primitives
  Magnum::Trade
  MagnumIntegration::ImGui
  MagnumPlugins::AssimpImporter
  MagnumPlugins::GltfImporter
  MagnumPlugins::StbImageConverter
  MagnumPlugins::StbImageImporter
  MagnumPlugins::WebPImporter
)

include(Magnum.cmake)

add_library(
  engine STATIC
  ${CMAKE_CURRENT_SOURCE_DIR}/src/camera.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/env.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/image.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/loader.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/shader.cpp
)

target_include_directories(engine PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

target_link_libraries(
  engine PUBLIC magnum spdlog::spdlog cpptrace::cpptrace
                tinyfiledialogs::tinyfiledialogs
)
target_compile_options(engine PUBLIC -fsanitize=address,undefined)
target_link_options(
  engine PUBLIC -fsanitize=address,undefined -static-libasan -static-libubsan
)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
  add_executable(viewer ./src/viewer.cpp)
  target_link_libraries(viewer PRIVATE engine)
endif()
