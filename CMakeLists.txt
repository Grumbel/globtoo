cmake_minimum_required(VERSION 3.16)
project(globe LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(PkgConfig REQUIRED)
pkg_check_modules(SDL3 REQUIRED sdl3)

find_library(GLES2_LIB GLESv2 REQUIRED)
find_library(EGL_LIB EGL REQUIRED)

add_executable(globe
  src/main.cpp
  src/globe.cpp
  src/camera.cpp
  src/input.cpp
  src/mesh.cpp
  src/shader.cpp
  src/texture.cpp
  src/math.cpp
)

target_include_directories(globe PRIVATE
  ${SDL3_INCLUDE_DIRS}
  ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(globe PRIVATE
  ${SDL3_LIBRARIES}
  ${GLES2_LIB}
  ${EGL_LIB}
  m
)

target_compile_options(globe PRIVATE -Wall -Wextra -O2)

# Copy assets next to binary for development
add_custom_command(TARGET globe POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_SOURCE_DIR}/assets $<TARGET_FILE_DIR:globe>/assets
  COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_SOURCE_DIR}/shaders $<TARGET_FILE_DIR:globe>/shaders
)
