cmake_minimum_required(VERSION 3.16)
# VERSION is the only source of the base version. Package builds (Nix) inject
# the complete development identifier via PROJECT_VERSION_FULL.
if(NOT DEFINED PROJECT_VERSION_FULL)
  file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/VERSION" _globtoo_base LIMIT_COUNT 1)
  set(PROJECT_VERSION_FULL "${_globtoo_base}")
  if(_globtoo_base MATCHES "-dev")
    find_package(Git QUIET)
    set(_globtoo_count "0")
    set(_globtoo_hash "dirty")
    if(GIT_FOUND AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/.git")
      execute_process(COMMAND "${GIT_EXECUTABLE}" rev-list --count HEAD
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        OUTPUT_VARIABLE _count OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET RESULT_VARIABLE _count_result)
      execute_process(COMMAND "${GIT_EXECUTABLE}" rev-parse --short=8 HEAD
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        OUTPUT_VARIABLE _hash OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET RESULT_VARIABLE _hash_result)
      execute_process(COMMAND "${GIT_EXECUTABLE}" status --porcelain
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        OUTPUT_VARIABLE _git_changes OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET)
      if(_count_result EQUAL 0 AND _count MATCHES "^[0-9]+$")
        set(_globtoo_count "${_count}")
      endif()
      if(_hash_result EQUAL 0 AND _hash MATCHES "^[0-9a-fA-F]+$")
        set(_globtoo_hash "${_hash}")
        if(NOT _git_changes STREQUAL "")
          string(APPEND _globtoo_hash "-dirty")
        endif()
      endif()
    endif()
    set(PROJECT_VERSION_FULL "${_globtoo_base}.${_globtoo_count}+g${_globtoo_hash}")
  endif()
endif()
string(REGEX MATCH "^[0-9]+(\\.[0-9]+)*" _globtoo_numeric_version "${PROJECT_VERSION_FULL}")
if(NOT _globtoo_numeric_version)
  message(FATAL_ERROR "VERSION must begin with a numeric version: ${PROJECT_VERSION_FULL}")
endif()
project(globtoo VERSION "${_globtoo_numeric_version}" LANGUAGES CXX)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/VERSION")
message(STATUS "Globtoo version: ${PROJECT_VERSION_FULL}")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(GLOBTOO_BUILD_APP "Build the SDL3 + GLES2 globe viewer" ON)
find_path(GEOS_INCLUDE_DIR geos_c.h REQUIRED)
find_library(GEOS_C_LIB geos_c REQUIRED)

if(GLOBTOO_BUILD_APP)
find_package(PkgConfig REQUIRED)
pkg_check_modules(SDL3 REQUIRED sdl3)
find_library(GLES2_LIB GLESv2 REQUIRED)
find_library(EGL_LIB EGL REQUIRED)

add_executable(globtoo
  src/main.cpp
  src/globe.cpp
  src/camera.cpp
  src/input.cpp
  src/mesh.cpp
  src/shader.cpp
  src/texture.cpp
  src/stars.cpp
  src/countries.cpp
  src/country_geometry.cpp
  src/country_mesh_io.cpp
  src/math.cpp
  src/map_view.cpp
  src/scene.cpp
  src/ui.cpp
  src/markers.cpp
)

target_include_directories(globtoo PRIVATE
  ${SDL3_INCLUDE_DIRS}
  ${GEOS_INCLUDE_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}/src
  ${CMAKE_CURRENT_SOURCE_DIR}/external
)

target_link_libraries(globtoo PRIVATE
  ${SDL3_LIBRARIES}
  ${GEOS_C_LIB}
  ${GLES2_LIB}
  ${EGL_LIB}
  m
)

target_compile_options(globtoo PRIVATE -Wall -Wextra -O2)
target_compile_definitions(globtoo PRIVATE GLOBTOO_VERSION="${PROJECT_VERSION_FULL}")
endif()

# Triangulate both Natural Earth resolutions while building, not at startup.
# In Nix these GeoJSON inputs are pinned fetchurl derivations injected by
# postPatch. A plain local CMake build also works with no downloaded maps:
# it can use the legacy assets/countries.geojson at runtime.
add_executable(country_mesh_compiler
  tools/build_country_mesh.cpp src/country_geometry.cpp src/country_mesh_io.cpp)
target_include_directories(country_mesh_compiler PRIVATE src ${GEOS_INCLUDE_DIR})
target_link_libraries(country_mesh_compiler PRIVATE ${GEOS_C_LIB})
target_compile_options(country_mesh_compiler PRIVATE -Wall -Wextra -O2)

set(COUNTRY_MESH_OUTPUTS)
foreach(scale 50m 10m)
  set(input "${CMAKE_SOURCE_DIR}/assets/countries_${scale}.geojson")
  if(EXISTS "${input}")
    set(output "${CMAKE_BINARY_DIR}/generated-assets/countries_${scale}.gmesh")
    add_custom_command(OUTPUT "${output}"
      COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/generated-assets"
      COMMAND $<TARGET_FILE:country_mesh_compiler> "${input}" "${output}"
      DEPENDS country_mesh_compiler "${input}"
      COMMENT "Triangulating Natural Earth ${scale} country boundaries"
      VERBATIM)
    list(APPEND COUNTRY_MESH_OUTPUTS "${output}")
  endif()
endforeach()
if(COUNTRY_MESH_OUTPUTS)
  add_custom_target(country_meshes ALL DEPENDS ${COUNTRY_MESH_OUTPUTS})
  if(GLOBTOO_BUILD_APP)
    add_dependencies(globtoo country_meshes)
  endif()
endif()

# Copy assets next to binary for development
if(GLOBTOO_BUILD_APP)
add_custom_command(TARGET globtoo POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_SOURCE_DIR}/assets $<TARGET_FILE_DIR:globtoo>/assets
  COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_SOURCE_DIR}/shaders $<TARGET_FILE_DIR:globtoo>/shaders
)
if(COUNTRY_MESH_OUTPUTS)
  add_custom_command(TARGET globtoo POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
      ${CMAKE_BINARY_DIR}/generated-assets $<TARGET_FILE_DIR:globtoo>/assets)
endif()
endif()

include(CTest)
if(BUILD_TESTING)
  if(GLOBTOO_BUILD_APP)
    add_executable(input_mouse_test
      tests/input_mouse_test.cpp src/input.cpp src/map_view.cpp src/camera.cpp)
    target_include_directories(input_mouse_test PRIVATE src ${SDL3_INCLUDE_DIRS})
    target_link_libraries(input_mouse_test PRIVATE ${SDL3_LIBRARIES})
    target_compile_options(input_mouse_test PRIVATE -Wall -Wextra -UNDEBUG)
    add_test(NAME input_mouse COMMAND input_mouse_test)
  endif()
  add_executable(markers_test tests/markers_test.cpp src/markers.cpp)
  target_include_directories(markers_test PRIVATE src)
  target_compile_options(markers_test PRIVATE -Wall -Wextra -UNDEBUG)
  add_test(NAME markers COMMAND markers_test)
  add_executable(grid_metrics_test tests/grid_metrics_test.cpp)
  target_include_directories(grid_metrics_test PRIVATE src)
  target_compile_options(grid_metrics_test PRIVATE -Wall -Wextra -UNDEBUG)
  add_test(NAME grid_metrics COMMAND grid_metrics_test)
  add_executable(layer_composition_test tests/layer_composition_test.cpp)
  target_include_directories(layer_composition_test PRIVATE src)
  target_compile_options(layer_composition_test PRIVATE -Wall -Wextra -UNDEBUG)
  add_test(NAME layer_composition COMMAND layer_composition_test)
  # ES 2.0 precision is part of the shader interface: mismatched vertex/
  # fragment defaults can compile separately yet fail at program link time.
  add_executable(shader_interface_test tests/shader_interface_test.cpp)
  target_compile_definitions(shader_interface_test PRIVATE
    GLOBTOO_SHADER_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/shaders")
  add_test(NAME shader_interface COMMAND shader_interface_test)

  add_executable(country_geometry_test
    tests/country_geometry_test.cpp src/country_geometry.cpp)
  target_include_directories(country_geometry_test PRIVATE src ${GEOS_INCLUDE_DIR})
  target_link_libraries(country_geometry_test PRIVATE ${GEOS_C_LIB})
  target_compile_options(country_geometry_test PRIVATE -Wall -Wextra -UNDEBUG)
  add_test(NAME country_geometry COMMAND country_geometry_test)

  add_executable(country_mesh_io_test
    tests/country_mesh_io_test.cpp src/country_mesh_io.cpp src/country_geometry.cpp)
  target_include_directories(country_mesh_io_test PRIVATE src ${GEOS_INCLUDE_DIR})
  target_link_libraries(country_mesh_io_test PRIVATE ${GEOS_C_LIB})
  target_compile_options(country_mesh_io_test PRIVATE -Wall -Wextra -UNDEBUG)
  add_test(NAME country_mesh_io COMMAND country_mesh_io_test)

  add_executable(coordinates_test tests/coordinates_test.cpp src/camera.cpp)
  add_executable(map_view_test tests/map_view_test.cpp src/map_view.cpp src/camera.cpp)
  target_include_directories(map_view_test PRIVATE src)
  target_compile_options(map_view_test PRIVATE -Wall -Wextra -UNDEBUG)
  add_test(NAME map_view COMMAND map_view_test)
  target_include_directories(coordinates_test PRIVATE src)
  target_compile_options(coordinates_test PRIVATE -Wall -Wextra -UNDEBUG)
  add_test(NAME coordinates COMMAND coordinates_test)
endif()
