cmake_minimum_required(VERSION 3.16)
project(globtoo LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(PkgConfig REQUIRED)
pkg_check_modules(SDL3 REQUIRED sdl3)
find_path(GEOS_INCLUDE_DIR geos_c.h REQUIRED)
find_library(GEOS_C_LIB geos_c REQUIRED)

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
  src/math.cpp
)

target_include_directories(globtoo PRIVATE
  ${SDL3_INCLUDE_DIRS}
  ${GEOS_INCLUDE_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(globtoo PRIVATE
  ${SDL3_LIBRARIES}
  ${GEOS_C_LIB}
  ${GLES2_LIB}
  ${EGL_LIB}
  m
)

target_compile_options(globtoo PRIVATE -Wall -Wextra -O2)

# Copy assets next to binary for development
add_custom_command(TARGET globtoo POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_SOURCE_DIR}/assets $<TARGET_FILE_DIR:globtoo>/assets
  COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_SOURCE_DIR}/shaders $<TARGET_FILE_DIR:globtoo>/shaders
)

include(CTest)
if(BUILD_TESTING)
  add_executable(country_geometry_test
    tests/country_geometry_test.cpp src/country_geometry.cpp)
  target_include_directories(country_geometry_test PRIVATE src ${GEOS_INCLUDE_DIR})
  target_link_libraries(country_geometry_test PRIVATE ${GEOS_C_LIB})
  target_compile_options(country_geometry_test PRIVATE -Wall -Wextra)
  add_test(NAME country_geometry COMMAND country_geometry_test)
endif()
