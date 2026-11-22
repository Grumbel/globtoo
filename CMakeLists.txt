# SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later

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
include(GNUInstallDirs)

option(GLOBTOO_BUILD_APP "Build the SDL3 + GLES2 globe viewer" ON)
# Nix supplies the pinned fetchurl output here. Keep imagery out of the source
# tree and never infer an Earth texture from a random file in assets/.
set(GLOBTOO_EARTH_TEXTURE "" CACHE FILEPATH "Real equirectangular Earth image to package (e.g. a Nix fetchurl output)")
if(GLOBTOO_EARTH_TEXTURE AND NOT EXISTS "${GLOBTOO_EARTH_TEXTURE}")
  message(FATAL_ERROR "GLOBTOO_EARTH_TEXTURE does not exist: ${GLOBTOO_EARTH_TEXTURE}")
endif()
if(GLOBTOO_BUILD_APP AND NOT GLOBTOO_EARTH_TEXTURE)
  message(STATUS "No Earth image configured; globe will use solid color unless --texture is specified")
endif()
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
  src/resources.cpp
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
target_compile_definitions(globtoo PRIVATE
  GLOBTOO_VERSION="${PROJECT_VERSION_FULL}"
  GLOBTOO_INSTALL_DATADIR="${CMAKE_INSTALL_DATADIR}"
  GLOBTOO_INSTALL_RESOURCE_DIR="${CMAKE_INSTALL_FULL_DATADIR}/globtoo")
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

# Stage only explicitly approved runtime assets beside a build-tree binary.
# Never copy all of assets/: that could reintroduce an old placeholder image
# or ship bulky GeoJSON source data by accident.
if(GLOBTOO_BUILD_APP)
  add_custom_command(TARGET globtoo POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:globtoo>/assets"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "${CMAKE_SOURCE_DIR}/assets/globtoo-icon.png" "$<TARGET_FILE_DIR:globtoo>/assets/globtoo-icon.png"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
      "${CMAKE_SOURCE_DIR}/shaders" "$<TARGET_FILE_DIR:globtoo>/shaders"
    # Also clear a leftover placeholder after reconfiguring an old build tree.
    COMMAND ${CMAKE_COMMAND} -E rm -f "$<TARGET_FILE_DIR:globtoo>/assets/earth.jpg"
    VERBATIM)
  if(GLOBTOO_EARTH_TEXTURE)
    add_custom_command(TARGET globtoo POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${GLOBTOO_EARTH_TEXTURE}" "$<TARGET_FILE_DIR:globtoo>/assets/earth.jpg"
      VERBATIM)
  endif()
  if(EXISTS "${CMAKE_SOURCE_DIR}/assets/countries.geojson")
    add_custom_command(TARGET globtoo POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_SOURCE_DIR}/assets/countries.geojson" "$<TARGET_FILE_DIR:globtoo>/assets/countries.geojson"
      VERBATIM)
  endif()
  if(COUNTRY_MESH_OUTPUTS)
    add_custom_command(TARGET globtoo POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_BINARY_DIR}/generated-assets" "$<TARGET_FILE_DIR:globtoo>/assets"
      VERBATIM)
  endif()
endif()

# Standard installation: no current-working-directory assumptions or Nix-only
# shell copy logic. The GeoJSON sources stay out of the package; only their
# compiled, versioned binary meshes are needed at runtime.
if(GLOBTOO_BUILD_APP)
  install(TARGETS globtoo RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")
  install(DIRECTORY "${CMAKE_SOURCE_DIR}/shaders/"
    DESTINATION "${CMAKE_INSTALL_DATADIR}/globtoo/shaders")
  install(FILES "${CMAKE_SOURCE_DIR}/assets/globtoo-icon.png"
    DESTINATION "${CMAKE_INSTALL_DATADIR}/globtoo/assets")
  # Only the explicitly configured real Earth image may be installed as
  # earth.jpg. In particular, never package a source-tree placeholder.
  if(GLOBTOO_EARTH_TEXTURE)
    install(FILES "${GLOBTOO_EARTH_TEXTURE}"
      DESTINATION "${CMAKE_INSTALL_DATADIR}/globtoo/assets"
      RENAME "earth.jpg")
  endif()
  if(EXISTS "${CMAKE_SOURCE_DIR}/assets/countries.geojson")
    install(FILES "${CMAKE_SOURCE_DIR}/assets/countries.geojson"
      DESTINATION "${CMAKE_INSTALL_DATADIR}/globtoo/assets")
  endif()
  if(COUNTRY_MESH_OUTPUTS)
    install(FILES ${COUNTRY_MESH_OUTPUTS}
      DESTINATION "${CMAKE_INSTALL_DATADIR}/globtoo/assets")
  endif()
  install(FILES "${CMAKE_SOURCE_DIR}/data/io.github.Grumbel.globtoo.desktop"
    DESTINATION "${CMAKE_INSTALL_DATADIR}/applications")
  install(FILES "${CMAKE_SOURCE_DIR}/data/io.github.Grumbel.globtoo.metainfo.xml"
    DESTINATION "${CMAKE_INSTALL_DATADIR}/metainfo")
  foreach(size 32 48 64 128 256)
    install(FILES "${CMAKE_SOURCE_DIR}/data/icons/globtoo-${size}.png"
      RENAME "globtoo.png"
      DESTINATION "${CMAKE_INSTALL_DATADIR}/icons/hicolor/${size}x${size}/apps")
  endforeach()
  configure_file("${CMAKE_SOURCE_DIR}/data/globtoo.1.in"
    "${CMAKE_BINARY_DIR}/globtoo.1" @ONLY)
  install(FILES "${CMAKE_BINARY_DIR}/globtoo.1"
    DESTINATION "${CMAKE_INSTALL_MANDIR}/man1")
  install(FILES "${CMAKE_SOURCE_DIR}/LICENSE" "${CMAKE_SOURCE_DIR}/README.md"
    "${CMAKE_SOURCE_DIR}/data/CREDITS.md"
    DESTINATION "${CMAKE_INSTALL_DOCDIR}")
  # Ship license texts for the GPL application and the MIT/CC0 components.
  install(DIRECTORY "${CMAKE_SOURCE_DIR}/LICENSES/"
    DESTINATION "${CMAKE_INSTALL_DOCDIR}/LICENSES"
    FILES_MATCHING PATTERN "*.txt")
  install(DIRECTORY "${CMAKE_SOURCE_DIR}/docs/"
    DESTINATION "${CMAKE_INSTALL_DOCDIR}/docs"
    FILES_MATCHING PATTERN "*.md")
endif()

include(CTest)
if(BUILD_TESTING)
  add_executable(resources_test tests/resources_test.cpp src/resources.cpp)
  target_include_directories(resources_test PRIVATE src)
  # These tests use assert(); keep the checks enabled in Release builds.
  target_compile_options(resources_test PRIVATE -UNDEBUG)
  target_compile_definitions(resources_test PRIVATE
    GLOBTOO_INSTALL_DATADIR="${CMAKE_INSTALL_DATADIR}")
  add_test(NAME resources COMMAND resources_test)
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
