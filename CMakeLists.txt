# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#
# Top-level build for the nslc compiler.
#
# Constitution Build/Code/Licensing: C++17 hard-pinned (C++20 forbidden
# until a constitutional amendment lifts the freeze). LLVM/CIRCT
# coding conventions enforced via .clang-format + .clang-tidy.

cmake_minimum_required(VERSION 3.22)

project(nslc
  VERSION 0.0.0
  LANGUAGES CXX
  DESCRIPTION "NSL hardware-description-language compiler"
  HOMEPAGE_URL "https://github.com/REPLACE-OWNER/nslc")

# -----------------------------------------------------------------------------
# Standards & defaults
# -----------------------------------------------------------------------------

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Default to Release if the user did not pick a build type. Using the
# generator-expression form so multi-config generators are unaffected.
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release CACHE STRING
    "Build type (Debug|Release|RelWithDebInfo|MinSizeRel)" FORCE)
endif()

# -----------------------------------------------------------------------------
# Project options
# -----------------------------------------------------------------------------

option(NSL_BUILD_TESTS  "Build the nslc test suites (lit + GoogleTest)" ON)

# -----------------------------------------------------------------------------
# Module path & helper modules
# -----------------------------------------------------------------------------

list(PREPEND CMAKE_MODULE_PATH
  "${CMAKE_CURRENT_SOURCE_DIR}/cmake"
  "${CMAKE_CURRENT_SOURCE_DIR}/cmake/modules")

include(NSLDeterminism)
include(NSLVersion)
include(AddNSLLibrary)

# -----------------------------------------------------------------------------
# External dependencies
# -----------------------------------------------------------------------------
#
# Per research.md §2, LLVM + MLIR + CIRCT are consumed as a vendored
# prebuilt install. Both packages publish standard `*Config.cmake`
# files; pass `-DMLIR_DIR=...` and `-DCIRCT_DIR=...` at configure time.

find_package(MLIR  REQUIRED CONFIG)
find_package(CIRCT REQUIRED CONFIG)

message(STATUS "Found MLIR  ${MLIR_PACKAGE_VERSION} at ${MLIR_DIR}")
message(STATUS "Found CIRCT ${CIRCT_PACKAGE_VERSION} at ${CIRCT_DIR}")

# -----------------------------------------------------------------------------
# Subdirectories
# -----------------------------------------------------------------------------

add_subdirectory(lib)
add_subdirectory(tools)

if(NSL_BUILD_TESTS)
  enable_testing()
  add_subdirectory(test)
  add_subdirectory(test_unit)
endif()
