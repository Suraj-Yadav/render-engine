include(FetchContent)

macro(_magnum_setup target)
  FetchContent_Declare(
    ${target}
    GIT_REPOSITORY https://github.com/mosra/${target}.git
    GIT_TAG master
    SYSTEM
  )
  FetchContent_MakeAvailable(${target})
  set(CMAKE_MODULE_PATH "${${target}_SOURCE_DIR}/modules" ${CMAKE_MODULE_PATH})
endmacro()

function(_compute_parts OUT1 OUT2 INPUT)
  string(FIND ${FEATURE} "::" INDEX)
  string(SUBSTRING ${FEATURE} 0 ${INDEX} val1)
  math(EXPR INDEX "${INDEX}+2")
  string(SUBSTRING ${FEATURE} ${INDEX}+2 -1 val2)

  # Write them back to the caller’s variables
  set(${OUT1}
      "${val1}"
      PARENT_SCOPE
  )
  set(${OUT2}
      "${val2}"
      PARENT_SCOPE
  )
endfunction()

set(CORRADE_CPU_USE_IFUNC OFF)
set(MAGNUM_BUILD_STATIC ON)
set(MAGNUM_BUILD_PLUGINS_STATIC ON)

foreach(FEATURE IN LISTS MAGNUM_FEATURES)
  _compute_parts(NAMESPACE LIBRARY ${FEATURE})

  string(PREPEND LIBRARY "MAGNUM_WITH_")
  string(TOUPPER ${LIBRARY} LIBRARY)
  set(${LIBRARY} ON)
endforeach()

_magnum_setup(corrade)
_magnum_setup(magnum)
_magnum_setup(magnum-integration)
_magnum_setup(magnum-plugins)

add_library(magnum INTERFACE)

foreach(FEATURE IN LISTS MAGNUM_FEATURES)
  _compute_parts(NAMESPACE LIBRARY ${FEATURE})
  find_package(${NAMESPACE} REQUIRED ${LIBRARY})
  target_link_libraries(magnum INTERFACE ${FEATURE})
endforeach()

target_link_libraries(magnum INTERFACE Corrade::Containers)
