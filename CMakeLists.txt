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
option(GLOBTOO_ENABLE_DOWNLOADS "Enable explicit libcurl/SSL asset download command" OFF)
if(GLOBTOO_ENABLE_DOWNLOADS)
  # pkg-config carries the transitive link dependencies of static libcurl
  # installations as well as the usual shared-library configuration.
  find_package(PkgConfig REQUIRED)
  pkg_check_modules(LIBCURL REQUIRED IMPORTED_TARGET libcurl)
  find_package(OpenSSL REQUIRED COMPONENTS Crypto)
  add_library(globtoo_asset_download STATIC src/asset_download.cpp)
  target_include_directories(globtoo_asset_download PUBLIC "${CMAKE_SOURCE_DIR}/src")
  target_link_libraries(globtoo_asset_download PUBLIC PkgConfig::LIBCURL OpenSSL::Crypto)
  target_compile_options(globtoo_asset_download PRIVATE -Wall -Wextra)
endif()

# Nix supplies the pinned fetchurl output here. Keep imagery out of the source
# tree and never infer an Earth texture from a random file in assets/.
set(GLOBTOO_EARTH_TEXTURE "" CACHE FILEPATH "Real equirectangular Earth image to package (e.g. a Nix fetchurl output)")
# Optional higher-resolution *imagery*, unlike the independently selected
# visual relief layer. Build-time tiles, not a huge runtime GL texture.
set(GLOBTOO_IMAGERY_TEXTURE "" CACHE FILEPATH "High-resolution equirectangular Earth imagery to tile for close zoom")
# Optional full-world topographic/bathymetric source, converted to GLES2-safe
# 1024px tiles at build time. Never package the untiled source image.
set(GLOBTOO_RELIEF_TEXTURE "" CACHE FILEPATH "NASA Blue Marble topography/bathymetry raster (north-up equirectangular)")
# NOAA ETOPO1 signed 16-bit, metre-valued elevation/bathymetry, optionally
# supplied by the packager. Unlike the visual Relief, the source is numeric.
set(GLOBTOO_ELEVATION_RAW "" CACHE FILEPATH "Headerless signed i16 little-endian worldwide elevation in metres")
set(GLOBTOO_ELEVATION_WIDTH 21601 CACHE STRING "Elevation source columns (ETOPO1 grid-registered default)")
set(GLOBTOO_ELEVATION_HEIGHT 10801 CACHE STRING "Elevation source rows (ETOPO1 grid-registered default)")
# Optional measured density: GPWv4-style global EPSG:4326 single-band
# GeoTIFF (rasterio), or a raw little-endian float32 2:1 grid. Both carry
# people/km2; do not accept an unlabeled population-count raster.
set(GLOBTOO_POPULATION_RASTER "" CACHE FILEPATH "Global density GeoTIFF or float32 raw in people/km2")
set(GLOBTOO_POPULATION_YEAR "" CACHE STRING "Required reference year of population density estimates")
set(GLOBTOO_POPULATION_ATTRIBUTION "" CACHE STRING "Required source attribution for population dataset")
set(GLOBTOO_POPULATION_WIDTH "" CACHE STRING "Raw float32 width (GeoTIFF reads georeferencing)")
set(GLOBTOO_POPULATION_HEIGHT "" CACHE STRING "Raw float32 height (GeoTIFF reads georeferencing)")
if(GLOBTOO_POPULATION_RASTER AND NOT EXISTS "${GLOBTOO_POPULATION_RASTER}")
  message(FATAL_ERROR "GLOBTOO_POPULATION_RASTER does not exist: ${GLOBTOO_POPULATION_RASTER}")
endif()
if(GLOBTOO_POPULATION_RASTER AND
   (NOT GLOBTOO_POPULATION_YEAR OR NOT GLOBTOO_POPULATION_ATTRIBUTION))
  message(FATAL_ERROR "GLOBTOO_POPULATION_RASTER requires GLOBTOO_POPULATION_YEAR and GLOBTOO_POPULATION_ATTRIBUTION")
endif()

if(GLOBTOO_ELEVATION_RAW AND NOT EXISTS "${GLOBTOO_ELEVATION_RAW}")
  message(FATAL_ERROR "GLOBTOO_ELEVATION_RAW does not exist: ${GLOBTOO_ELEVATION_RAW}")
endif()

if(GLOBTOO_IMAGERY_TEXTURE AND NOT EXISTS "${GLOBTOO_IMAGERY_TEXTURE}")
  message(FATAL_ERROR "GLOBTOO_IMAGERY_TEXTURE does not exist: ${GLOBTOO_IMAGERY_TEXTURE}")
endif()
if(GLOBTOO_RELIEF_TEXTURE AND NOT EXISTS "${GLOBTOO_RELIEF_TEXTURE}")
  message(FATAL_ERROR "GLOBTOO_RELIEF_TEXTURE does not exist: ${GLOBTOO_RELIEF_TEXTURE}")
endif()
if(GLOBTOO_EARTH_TEXTURE AND NOT EXISTS "${GLOBTOO_EARTH_TEXTURE}")
  message(FATAL_ERROR "GLOBTOO_EARTH_TEXTURE does not exist: ${GLOBTOO_EARTH_TEXTURE}")
endif()
# Optional, explicitly sourced global planetary mosaics. Each source must be
# north-up, 360x180-degree equirectangular; the preparer converts its declared
# longitude axis to Globtoo's east-positive -180..180 UV convention.
# No default URL or hash is invented: a downstream packager must supply and
# check the original file and its redistribution terms.
set(GLOBTOO_PLANET_MAX_WIDTH 2048 CACHE STRING
    "Maximum width of normalized Moon/Mars/Io texture for GLES2")
set(PLANET_TEXTURE_OUTPUTS)
set(PLANET_IMAGERY_OUTPUTS)
foreach(body moon mars io)
  string(TOUPPER "${body}" upper)
  set(GLOBTOO_${upper}_TEXTURE "" CACHE FILEPATH "Optional ${body} global plate-carree texture")
  set(GLOBTOO_${upper}_IMAGERY_TEXTURE "" CACHE FILEPATH
      "Optional high-resolution ${body} global mosaic to tile instead of uploading a huge GLES2 texture")
  set(GLOBTOO_${upper}_LONGITUDE_POSITIVE "" CACHE STRING "Source direction: east or west")
  set(GLOBTOO_${upper}_LONGITUDE_LEFT "" CACHE STRING "Source left boundary: 0 or 180 degrees")
  set(GLOBTOO_${upper}_SOURCE "" CACHE STRING "Source publication/URL for ${body} map")
  set(GLOBTOO_${upper}_LATITUDE_TYPE "" CACHE STRING "Source latitude: planetocentric or planetographic")
  set(GLOBTOO_${upper}_LICENSE "" CACHE STRING "Verified redistributable license for ${body} map")
  if(GLOBTOO_${upper}_TEXTURE OR GLOBTOO_${upper}_IMAGERY_TEXTURE)
    foreach(variable TEXTURE IMAGERY_TEXTURE)
      if(GLOBTOO_${upper}_${variable} AND NOT EXISTS "${GLOBTOO_${upper}_${variable}}")
        message(FATAL_ERROR "GLOBTOO_${upper}_${variable} does not exist: ${GLOBTOO_${upper}_${variable}}")
      endif()
    endforeach()
    if(NOT GLOBTOO_${upper}_SOURCE OR NOT GLOBTOO_${upper}_LICENSE OR
       NOT GLOBTOO_${upper}_LATITUDE_TYPE MATCHES "^(planetocentric|planetographic)$" OR
       NOT GLOBTOO_${upper}_LONGITUDE_POSITIVE MATCHES "^(east|west)$" OR
       NOT GLOBTOO_${upper}_LONGITUDE_LEFT MATCHES "^(0|180)$")
      message(FATAL_ERROR "${body} texture requires SOURCE, LICENSE, LATITUDE_TYPE=planetocentric|planetographic, LONGITUDE_POSITIVE=east|west and LONGITUDE_LEFT=0|180")
    endif()
    if(GLOBTOO_${upper}_TEXTURE AND
       (NOT GLOBTOO_PLANET_MAX_WIDTH MATCHES "^[0-9]+$" OR
        GLOBTOO_PLANET_MAX_WIDTH LESS 2))
      message(FATAL_ERROR "GLOBTOO_PLANET_MAX_WIDTH must be an even integer >= 2")
    endif()
    math(EXPR _planet_width_mod "${GLOBTOO_PLANET_MAX_WIDTH} % 2")
    if(GLOBTOO_${upper}_TEXTURE AND NOT _planet_width_mod EQUAL 0)
      message(FATAL_ERROR "GLOBTOO_PLANET_MAX_WIDTH must be even")
    endif()
    find_package(Python3 REQUIRED COMPONENTS Interpreter)
    execute_process(COMMAND ${Python3_EXECUTABLE} -c "from PIL import Image"
      RESULT_VARIABLE _planet_pillow ERROR_QUIET)
    if(NOT _planet_pillow EQUAL 0)
      message(FATAL_ERROR "Planet texture preprocessing requires Python Pillow")
    endif()
    if(GLOBTOO_${upper}_TEXTURE)
      set(_planet_output "${CMAKE_BINARY_DIR}/generated-planetary/${body}.jpg")
      add_custom_command(OUTPUT "${_planet_output}"
        COMMAND ${Python3_EXECUTABLE} "${CMAKE_SOURCE_DIR}/tools/normalize_planet_texture.py"
          "${GLOBTOO_${upper}_TEXTURE}" "${_planet_output}"
          --positive "${GLOBTOO_${upper}_LONGITUDE_POSITIVE}"
          --left "${GLOBTOO_${upper}_LONGITUDE_LEFT}"
          --latitude "${GLOBTOO_${upper}_LATITUDE_TYPE}"
          --body "${body}"
          --max-width "${GLOBTOO_PLANET_MAX_WIDTH}"
        DEPENDS "${GLOBTOO_${upper}_TEXTURE}"
          "${CMAKE_SOURCE_DIR}/tools/normalize_planet_texture.py"
          "${CMAKE_SOURCE_DIR}/tools/planetary_latitudes.py"
        COMMENT "Normalizing ${body} global raster" VERBATIM)
      add_custom_target(${body}_texture ALL DEPENDS "${_planet_output}")
      list(APPEND PLANET_TEXTURE_OUTPUTS "${body}")
    endif()
    # These records are installed with the normalized imagery; they explicitly
    # preserve the provenance/longitude semantics of the external source.
    file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/generated-planetary")
    file(WRITE "${CMAKE_BINARY_DIR}/generated-planetary/${body}.source.txt"
      "Source: ${GLOBTOO_${upper}_SOURCE}\nLicense: ${GLOBTOO_${upper}_LICENSE}\n"
      "Original longitude direction: ${GLOBTOO_${upper}_LONGITUDE_POSITIVE}\n"
      "Original left longitude: ${GLOBTOO_${upper}_LONGITUDE_LEFT} degrees\n"
      "Source latitude type: ${GLOBTOO_${upper}_LATITUDE_TYPE}\n"
      "Output: north-up planetocentric east-positive -180..180 equirectangular\n"
      "Latitude conversion: ${body} reference surface, when applicable\n")
    if(GLOBTOO_${upper}_IMAGERY_TEXTURE)
      set(_planet_tiles_dir "${CMAKE_BINARY_DIR}/generated-imagery/${body}")
      file(MAKE_DIRECTORY "${_planet_tiles_dir}")
      configure_file("${CMAKE_BINARY_DIR}/generated-planetary/${body}.source.txt"
                     "${_planet_tiles_dir}/source.txt" COPYONLY)
      set(_planet_tiles_manifest "${_planet_tiles_dir}/manifest.txt")
      add_custom_command(OUTPUT "${_planet_tiles_manifest}"
        COMMAND ${Python3_EXECUTABLE} "${CMAKE_SOURCE_DIR}/tools/build_raster_tiles.py"
          "${GLOBTOO_${upper}_IMAGERY_TEXTURE}" "${_planet_tiles_dir}"
          --positive "${GLOBTOO_${upper}_LONGITUDE_POSITIVE}"
          --left "${GLOBTOO_${upper}_LONGITUDE_LEFT}"
          --latitude "${GLOBTOO_${upper}_LATITUDE_TYPE}"
          --body "${body}"
        DEPENDS "${GLOBTOO_${upper}_IMAGERY_TEXTURE}"
          "${CMAKE_SOURCE_DIR}/tools/build_raster_tiles.py"
          "${CMAKE_SOURCE_DIR}/tools/planetary_latitudes.py"
        COMMENT "Preparing oriented high-resolution ${body} GLES2 tiles" VERBATIM)
      add_custom_target(${body}_imagery_tiles ALL DEPENDS "${_planet_tiles_manifest}")
      list(APPEND PLANET_IMAGERY_OUTPUTS "${body}")
    endif()
  endif()
endforeach()

if(GLOBTOO_BUILD_APP AND NOT GLOBTOO_EARTH_TEXTURE AND NOT GLOBTOO_IMAGERY_TEXTURE)
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
  src/geographic_features.cpp
  src/geographic_overlay.cpp
  src/tiled_raster.cpp
  src/raster_cache.cpp
  src/raster_legend.cpp
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

if(GLOBTOO_ENABLE_DOWNLOADS)
  target_compile_definitions(globtoo PRIVATE GLOBTOO_ENABLE_DOWNLOADS=1)
  target_link_libraries(globtoo PRIVATE globtoo_asset_download)
endif()

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

# Optional Natural Earth 10m populated places / river centerlines. Convert to
# one compact versioned binary file, so startup never parses huge GeoJSON.
# Local source builds can omit these files; Nix provides both pinned inputs.
set(GEOGRAPHIC_FEATURE_OUTPUT "")
if(EXISTS "${CMAKE_SOURCE_DIR}/assets/places_10m.geojson" AND
   EXISTS "${CMAKE_SOURCE_DIR}/assets/rivers_10m.geojson")
  find_package(Python3 REQUIRED COMPONENTS Interpreter)
  set(GEOGRAPHIC_FEATURE_OUTPUT "${CMAKE_BINARY_DIR}/generated-assets/geographic_10m.gfeat")
  add_custom_command(OUTPUT "${GEOGRAPHIC_FEATURE_OUTPUT}"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/generated-assets"
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_SOURCE_DIR}/tools/build_geographic_features.py"
      --places "${CMAKE_SOURCE_DIR}/assets/places_10m.geojson"
      --rivers "${CMAKE_SOURCE_DIR}/assets/rivers_10m.geojson"
      --output "${GEOGRAPHIC_FEATURE_OUTPUT}"
    DEPENDS "${CMAKE_SOURCE_DIR}/tools/build_geographic_features.py"
      "${CMAKE_SOURCE_DIR}/assets/places_10m.geojson"
      "${CMAKE_SOURCE_DIR}/assets/rivers_10m.geojson"
    COMMENT "Compiling Natural Earth 10m populated places and rivers"
    VERBATIM)
  add_custom_target(geographic_features_asset ALL DEPENDS "${GEOGRAPHIC_FEATURE_OUTPUT}")
  if(GLOBTOO_BUILD_APP)
    add_dependencies(globtoo geographic_features_asset)
  endif()
endif()

# True-color high-resolution imagery remains in the IMAGERY mode. It uses
# the same versioned tile layout as Relief, but a separate runtime cache.
# The original GLOBTOO_EARTH_TEXTURE is retained for distant views, morphs,
# and transient missing tiles. Only the generated tiles are installed.
set(IMAGERY_OUTPUT "")
if(GLOBTOO_IMAGERY_TEXTURE)
  find_package(Python3 REQUIRED COMPONENTS Interpreter)
  execute_process(COMMAND ${Python3_EXECUTABLE} -c "from PIL import Image"
    RESULT_VARIABLE _globtoo_imagery_pillow ERROR_QUIET)
  if(NOT _globtoo_imagery_pillow EQUAL 0)
    message(FATAL_ERROR "GLOBTOO_IMAGERY_TEXTURE requires Pillow (Python PIL)")
  endif()
  set(IMAGERY_OUTPUT "${CMAKE_BINARY_DIR}/generated-imagery/manifest.txt")
  add_custom_command(OUTPUT "${IMAGERY_OUTPUT}"
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_SOURCE_DIR}/tools/build_raster_tiles.py"
      "${GLOBTOO_IMAGERY_TEXTURE}" "${CMAKE_BINARY_DIR}/generated-imagery"
    DEPENDS "${GLOBTOO_IMAGERY_TEXTURE}" "${CMAKE_SOURCE_DIR}/tools/build_raster_tiles.py"
    COMMENT "Preparing high-resolution true-color Earth imagery tiles"
    VERBATIM)
  add_custom_target(imagery_tiles ALL DEPENDS "${IMAGERY_OUTPUT}")
  if(GLOBTOO_BUILD_APP)
    add_dependencies(globtoo imagery_tiles)
  endif()
endif()

# Prepare optional, high-resolution topography/bathymetry as power-of-two tiles.
# Pillow is only a build dependency when the layer source is supplied.  A
# normal CMake/Nix build has no extra requirements and uses the existing Earth
# imagery.  Large raster sources remain outside Git and the install closure.
set(RELIEF_OUTPUT "")
if(GLOBTOO_RELIEF_TEXTURE)
  find_package(Python3 REQUIRED COMPONENTS Interpreter)
  execute_process(COMMAND ${Python3_EXECUTABLE} -c "from PIL import Image"
    RESULT_VARIABLE _globtoo_pillow_result ERROR_QUIET)
  if(NOT _globtoo_pillow_result EQUAL 0)
    message(FATAL_ERROR "GLOBTOO_RELIEF_TEXTURE requires Pillow (Python PIL) at build time")
  endif()
  set(RELIEF_OUTPUT "${CMAKE_BINARY_DIR}/generated-relief/manifest.txt")
  add_custom_command(OUTPUT "${RELIEF_OUTPUT}"
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_SOURCE_DIR}/tools/build_raster_tiles.py"
      "${GLOBTOO_RELIEF_TEXTURE}" "${CMAKE_BINARY_DIR}/generated-relief"
    DEPENDS "${GLOBTOO_RELIEF_TEXTURE}"
      "${CMAKE_SOURCE_DIR}/tools/build_raster_tiles.py"
    COMMENT "Preparing high-resolution relief/bathymetry GLES2 tiles"
    VERBATIM)
  add_custom_target(relief_tiles ALL DEPENDS "${RELIEF_OUTPUT}")
  if(GLOBTOO_BUILD_APP)
    add_dependencies(globtoo relief_tiles)
  endif()
endif()

# Numeric signed-height source -> colorized tiles with physical-unit legend.
# Both numpy and Pillow are needed only for this optional preprocessing step.
set(ELEVATION_OUTPUT "")
if(GLOBTOO_ELEVATION_RAW)
  find_package(Python3 REQUIRED COMPONENTS Interpreter)
  execute_process(COMMAND ${Python3_EXECUTABLE} -c "import numpy; from PIL import Image"
    RESULT_VARIABLE _globtoo_numeric_result ERROR_QUIET)
  if(NOT _globtoo_numeric_result EQUAL 0)
    message(FATAL_ERROR "GLOBTOO_ELEVATION_RAW requires numpy and Pillow at build time")
  endif()
  set(ELEVATION_OUTPUT "${CMAKE_BINARY_DIR}/generated-elevation/manifest.txt")
  add_custom_command(OUTPUT "${ELEVATION_OUTPUT}"
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_SOURCE_DIR}/tools/build_elevation_tiles.py"
      "${GLOBTOO_ELEVATION_RAW}" "${CMAKE_BINARY_DIR}/generated-elevation"
      --width ${GLOBTOO_ELEVATION_WIDTH} --height ${GLOBTOO_ELEVATION_HEIGHT}
    DEPENDS "${GLOBTOO_ELEVATION_RAW}" "${CMAKE_SOURCE_DIR}/tools/build_elevation_tiles.py"
    COMMENT "Preparing numeric elevation/bathymetry tiles with a metre legend"
    VERBATIM)
  add_custom_target(elevation_tiles ALL DEPENDS "${ELEVATION_OUTPUT}")
  if(GLOBTOO_BUILD_APP)
    add_dependencies(globtoo elevation_tiles)
  endif()
endif()

# Optional population density numeric raster, stored as log-ramp color JPEG
# tiles with a tagged physical-unit legend. No runtime Python dependency.
set(POPULATION_OUTPUT "")
if(GLOBTOO_POPULATION_RASTER)
  find_package(Python3 REQUIRED COMPONENTS Interpreter)
  execute_process(COMMAND ${Python3_EXECUTABLE} -c "import numpy; from PIL import Image"
    RESULT_VARIABLE _globtoo_population_deps ERROR_QUIET)
  if(NOT _globtoo_population_deps EQUAL 0)
    message(FATAL_ERROR "Population preprocessing requires numpy and Pillow")
  endif()
  if(GLOBTOO_POPULATION_RASTER MATCHES "\\.[tT][iI][fF][fF]?$")
    execute_process(COMMAND ${Python3_EXECUTABLE} -c "import rasterio"
      RESULT_VARIABLE _globtoo_population_gdal ERROR_QUIET)
    if(NOT _globtoo_population_gdal EQUAL 0)
      message(FATAL_ERROR "GeoTIFF population preprocessing requires Python rasterio")
    endif()
  endif()
  set(_population_dimensions)
  if(GLOBTOO_POPULATION_WIDTH AND GLOBTOO_POPULATION_HEIGHT)
    list(APPEND _population_dimensions --width ${GLOBTOO_POPULATION_WIDTH}
      --height ${GLOBTOO_POPULATION_HEIGHT})
  endif()
  set(POPULATION_OUTPUT "${CMAKE_BINARY_DIR}/generated-population/manifest.txt")
  add_custom_command(OUTPUT "${POPULATION_OUTPUT}"
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_SOURCE_DIR}/tools/build_population_tiles.py"
      "${GLOBTOO_POPULATION_RASTER}" "${CMAKE_BINARY_DIR}/generated-population"
      --year ${GLOBTOO_POPULATION_YEAR}
      --attribution "${GLOBTOO_POPULATION_ATTRIBUTION}"
      ${_population_dimensions}
    DEPENDS "${GLOBTOO_POPULATION_RASTER}"
      "${CMAKE_SOURCE_DIR}/tools/build_population_tiles.py"
    COMMENT "Preparing measured population density (people/km2) tiles"
    VERBATIM)
  add_custom_target(population_tiles ALL DEPENDS "${POPULATION_OUTPUT}")
  if(GLOBTOO_BUILD_APP)
    add_dependencies(globtoo population_tiles)
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
  if(POPULATION_OUTPUT)
    add_custom_command(TARGET globtoo POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_BINARY_DIR}/generated-population" "$<TARGET_FILE_DIR:globtoo>/population"
      VERBATIM)
  endif()
  if(ELEVATION_OUTPUT)
    add_custom_command(TARGET globtoo POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_BINARY_DIR}/generated-elevation" "$<TARGET_FILE_DIR:globtoo>/elevation"
      VERBATIM)
  endif()
  if(IMAGERY_OUTPUT)
    add_custom_command(TARGET globtoo POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_BINARY_DIR}/generated-imagery" "$<TARGET_FILE_DIR:globtoo>/imagery"
      VERBATIM)
  endif()
  if(RELIEF_OUTPUT)
    add_custom_command(TARGET globtoo POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_BINARY_DIR}/generated-relief" "$<TARGET_FILE_DIR:globtoo>/relief"
      VERBATIM)
  endif()
  foreach(body IN LISTS PLANET_TEXTURE_OUTPUTS)
    add_dependencies(globtoo ${body}_texture)
    add_custom_command(TARGET globtoo POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_BINARY_DIR}/generated-planetary/${body}.jpg"
        "$<TARGET_FILE_DIR:globtoo>/assets/${body}.jpg"
      COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_BINARY_DIR}/generated-planetary/${body}.source.txt"
        "$<TARGET_FILE_DIR:globtoo>/assets/${body}.source.txt"
      VERBATIM)
  endforeach()
  foreach(body IN LISTS PLANET_IMAGERY_OUTPUTS)
    add_dependencies(globtoo ${body}_imagery_tiles)
    add_custom_command(TARGET globtoo POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:globtoo>/imagery/${body}"
      COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_BINARY_DIR}/generated-imagery/${body}"
        "$<TARGET_FILE_DIR:globtoo>/imagery/${body}"
      VERBATIM)
  endforeach()
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
  if(GEOGRAPHIC_FEATURE_OUTPUT)
    add_custom_command(TARGET globtoo POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${GEOGRAPHIC_FEATURE_OUTPUT}" "$<TARGET_FILE_DIR:globtoo>/assets/geographic_10m.gfeat"
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
  if(POPULATION_OUTPUT)
    install(DIRECTORY "${CMAKE_BINARY_DIR}/generated-population/"
      DESTINATION "${CMAKE_INSTALL_DATADIR}/globtoo/population")
  endif()
  if(ELEVATION_OUTPUT)
    install(DIRECTORY "${CMAKE_BINARY_DIR}/generated-elevation/"
      DESTINATION "${CMAKE_INSTALL_DATADIR}/globtoo/elevation")
  endif()
  if(IMAGERY_OUTPUT)
    install(DIRECTORY "${CMAKE_BINARY_DIR}/generated-imagery/"
      DESTINATION "${CMAKE_INSTALL_DATADIR}/globtoo/imagery")
  endif()
  if(RELIEF_OUTPUT)
    install(DIRECTORY "${CMAKE_BINARY_DIR}/generated-relief/"
      DESTINATION "${CMAKE_INSTALL_DATADIR}/globtoo/relief")
  endif()
  foreach(body IN LISTS PLANET_TEXTURE_OUTPUTS)
    install(FILES
      "${CMAKE_BINARY_DIR}/generated-planetary/${body}.jpg"
      "${CMAKE_BINARY_DIR}/generated-planetary/${body}.source.txt"
      DESTINATION "${CMAKE_INSTALL_DATADIR}/globtoo/assets")
  endforeach()
  foreach(body IN LISTS PLANET_IMAGERY_OUTPUTS)
    install(DIRECTORY "${CMAKE_BINARY_DIR}/generated-imagery/${body}/"
      DESTINATION "${CMAKE_INSTALL_DATADIR}/globtoo/imagery/${body}")
  endforeach()
  if(GLOBTOO_EARTH_TEXTURE)
    install(FILES "${GLOBTOO_EARTH_TEXTURE}"
      DESTINATION "${CMAKE_INSTALL_DATADIR}/globtoo/assets"
      RENAME "earth.jpg")
  endif()
  if(EXISTS "${CMAKE_SOURCE_DIR}/assets/countries.geojson")
    install(FILES "${CMAKE_SOURCE_DIR}/assets/countries.geojson"
      DESTINATION "${CMAKE_INSTALL_DATADIR}/globtoo/assets")
  endif()
  if(GEOGRAPHIC_FEATURE_OUTPUT)
    install(FILES "${GEOGRAPHIC_FEATURE_OUTPUT}"
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
  if(GLOBTOO_ENABLE_DOWNLOADS)
    add_executable(asset_download_test tests/asset_download_test.cpp)
    target_link_libraries(asset_download_test PRIVATE globtoo_asset_download)
    target_compile_options(asset_download_test PRIVATE -UNDEBUG)
    add_test(NAME asset_download COMMAND asset_download_test)
  endif()

  if(GLOBTOO_BUILD_APP)
    add_executable(input_mouse_test
      tests/input_mouse_test.cpp src/input.cpp src/map_view.cpp src/camera.cpp)
    target_include_directories(input_mouse_test PRIVATE src ${SDL3_INCLUDE_DIRS})
    target_link_libraries(input_mouse_test PRIVATE ${SDL3_LIBRARIES})
    target_compile_options(input_mouse_test PRIVATE -Wall -Wextra -UNDEBUG)
    add_test(NAME input_mouse COMMAND input_mouse_test)
    add_executable(ui_event_test tests/ui_event_test.cpp
      src/ui.cpp src/input.cpp src/camera.cpp src/map_view.cpp src/scene.cpp
      src/shader.cpp src/resources.cpp src/geographic_features.cpp)
    target_include_directories(ui_event_test PRIVATE src ${SDL3_INCLUDE_DIRS})
    target_link_libraries(ui_event_test PRIVATE ${SDL3_LIBRARIES} ${GLES2_LIB} m)
    target_compile_options(ui_event_test PRIVATE -Wall -Wextra -UNDEBUG)
    add_test(NAME ui_event COMMAND ui_event_test)
  endif()
  find_package(Python3 QUIET COMPONENTS Interpreter)
  if(Python3_Interpreter_FOUND)
    add_test(NAME geographic_compiler
      COMMAND ${Python3_EXECUTABLE} "${CMAKE_SOURCE_DIR}/tests/geographic_compiler_test.py")
  endif()
  add_executable(geographic_features_test tests/geographic_features_test.cpp
    src/geographic_features.cpp)
  target_include_directories(geographic_features_test PRIVATE src)
  target_compile_options(geographic_features_test PRIVATE -Wall -Wextra -UNDEBUG)
  add_test(NAME geographic_features COMMAND geographic_features_test)
  if(GEOGRAPHIC_FEATURE_OUTPUT)
    add_test(NAME geographic_features_compilation
      COMMAND geographic_features_test "${GEOGRAPHIC_FEATURE_OUTPUT}")
  endif()
  add_executable(raster_legend_test tests/raster_legend_test.cpp src/raster_legend.cpp)
  target_include_directories(raster_legend_test PRIVATE src)
  target_compile_options(raster_legend_test PRIVATE -Wall -Wextra -UNDEBUG)
  add_test(NAME raster_legend COMMAND raster_legend_test)
  if(POPULATION_OUTPUT)
    add_test(NAME raster_population_manifest COMMAND raster_tiles_test "${POPULATION_OUTPUT}")
    add_test(NAME raster_population_legend
      COMMAND raster_legend_test "${CMAKE_BINARY_DIR}/generated-population/legend.txt")
  endif()
  if(ELEVATION_OUTPUT)
    add_test(NAME raster_elevation_manifest COMMAND raster_tiles_test "${ELEVATION_OUTPUT}")
    add_test(NAME raster_elevation_legend
      COMMAND raster_legend_test "${CMAKE_BINARY_DIR}/generated-elevation/legend.txt")
  endif()
  add_executable(raster_tiles_test tests/raster_tiles_test.cpp src/tiled_raster.cpp)
  target_include_directories(raster_tiles_test PRIVATE src)
  target_compile_options(raster_tiles_test PRIVATE -Wall -Wextra -UNDEBUG)
  add_test(NAME raster_tiles COMMAND raster_tiles_test)
  if(IMAGERY_OUTPUT)
    add_test(NAME raster_imagery_manifest COMMAND raster_tiles_test "${IMAGERY_OUTPUT}")
  endif()
  if(RELIEF_OUTPUT)
    add_test(NAME raster_tiles_manifest COMMAND raster_tiles_test "${RELIEF_OUTPUT}")
  endif()
  if(Python3_Interpreter_FOUND)
    execute_process(COMMAND ${Python3_EXECUTABLE} -c "from PIL import Image"
      RESULT_VARIABLE _globtoo_test_pillow ERROR_QUIET)
    execute_process(COMMAND ${Python3_EXECUTABLE} -c "import numpy; from PIL import Image"
      RESULT_VARIABLE _globtoo_test_numpy ERROR_QUIET)
    if(_globtoo_test_numpy EQUAL 0)
      add_test(NAME population_tiles_compiler
        COMMAND ${CMAKE_COMMAND} -E env PYTHONPATH=${CMAKE_SOURCE_DIR}
          ${Python3_EXECUTABLE} "${CMAKE_SOURCE_DIR}/tests/population_tiles_compiler_test.py")
      add_test(NAME elevation_tiles_compiler
        COMMAND ${CMAKE_COMMAND} -E env PYTHONPATH=${CMAKE_SOURCE_DIR}
          ${Python3_EXECUTABLE} "${CMAKE_SOURCE_DIR}/tests/elevation_tiles_compiler_test.py")
    endif()
    if(_globtoo_test_pillow EQUAL 0)
      add_test(NAME prepare_planetary_map
        COMMAND ${CMAKE_COMMAND} -E env PYTHONPATH=${CMAKE_SOURCE_DIR}
          ${Python3_EXECUTABLE} "${CMAKE_SOURCE_DIR}/tests/prepare_planetary_map_test.py")
      add_test(NAME planetary_alignment
        COMMAND ${CMAKE_COMMAND} -E env PYTHONPATH=${CMAKE_SOURCE_DIR}
          ${Python3_EXECUTABLE} "${CMAKE_SOURCE_DIR}/tests/planetary_alignment_test.py")
      add_test(NAME planetary_texture_orientation
        COMMAND ${Python3_EXECUTABLE} "${CMAKE_SOURCE_DIR}/tests/planet_texture_test.py")
      add_test(NAME raster_tiles_compiler
        COMMAND ${CMAKE_COMMAND} -E env PYTHONPATH=${CMAKE_SOURCE_DIR}
          ${Python3_EXECUTABLE} "${CMAKE_SOURCE_DIR}/tests/raster_tiles_compiler_test.py")
      add_test(NAME planetary_latitudes
        COMMAND ${CMAKE_COMMAND} -E env PYTHONPATH=${CMAKE_SOURCE_DIR}
          ${Python3_EXECUTABLE} "${CMAKE_SOURCE_DIR}/tests/planetary_latitudes_test.py")
    endif()
  endif()
  add_executable(planetary_features_test tests/planetary_features_test.cpp)
  target_include_directories(planetary_features_test PRIVATE src)
  target_compile_definitions(planetary_features_test PRIVATE
    GLOBTOO_PLANETARY_FEATURES_TSV="${CMAKE_SOURCE_DIR}/data/planetary_features.tsv")
  target_compile_options(planetary_features_test PRIVATE -Wall -Wextra -UNDEBUG)
  add_test(NAME planetary_features COMMAND planetary_features_test)
  add_executable(bodies_test tests/bodies_test.cpp)
  target_include_directories(bodies_test PRIVATE src)
  target_compile_options(bodies_test PRIVATE -Wall -Wextra -UNDEBUG)
  add_test(NAME bodies COMMAND bodies_test)
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
