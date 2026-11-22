cmake_minimum_required(VERSION 3.16)
project(globtoo LANGUAGES CXX)

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
