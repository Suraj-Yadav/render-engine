include(FetchContent)

FetchContent_Declare(
  gltf-sample-renderer
  GIT_REPOSITORY https://github.com/KhronosGroup/glTF-Sample-Renderer
  GIT_TAG bec106e53da4a6a398aa3205f0f96563519a657e
  PATCH_COMMAND git reset --hard && git apply
                "${CMAKE_CURRENT_LIST_DIR}/shaders.diff"
)
FetchContent_MakeAvailable(gltf-sample-renderer)

set(GLTF_SHADER_DIR
    "/home/suraj/work/glTF-Sample-Renderer/source/Renderer/shaders"
)

configure_file(
  "${CMAKE_CURRENT_SOURCE_DIR}/cmake/shader_path.h.in"
  "${CMAKE_CURRENT_BINARY_DIR}/generated/shader_path.h"
)

add_library(gltf-shader INTERFACE)

target_include_directories(
  gltf-shader INTERFACE "${CMAKE_CURRENT_BINARY_DIR}/generated"
)
