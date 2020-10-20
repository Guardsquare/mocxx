cmake_minimum_required(VERSION 3.11 FATAL_ERROR)

project(Mocxx)

set(VERSION_MAJOR 0)
set(VERSION_MINOR 1)
set(VERSION_PATCH 0)

option(FORCE_COLORED_OUTPUT "ANSI-colored output" TRUE)

message(STATUS "${PROJECT_NAME} ${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_PATCH}")


# Some generators, e.g. Ninja, don't support automatic color capable terminal
# detection. So we support forcing colored diagnostics.
# Ninja rationale in https://github.com/ninja-build/ninja/issues/814
if (${FORCE_COLORED_OUTPUT})
  add_compile_options(-fdiagnostics-color=always)
endif()

set (CMAKE_CXX_STANDARD 17)
set (CMAKE_CXX_STANDARD_REQUIRED ON)

# Set project include dirs.
include_directories(${PROJECT_SOURCE_DIR}/include)

add_subdirectory(vendor)
add_subdirectory(test)
