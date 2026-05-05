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

# AddressSanitizer + LeakSanitizer (LSan rides along with ASan on
# Linux x86_64 by default; runtime is enabled via
# ASAN_OPTIONS=detect_leaks=1). UBSan is intentionally NOT bundled:
# the vendored LLVM/MLIR/CIRCT static libs at /opt/llvm + /opt/circt
# are built with `-fno-rtti` (LLVM default), and UBSan's `vptr`
# check needs typeinfo, so a combined `-fsanitize=address,undefined`
# build hits "undefined reference to typeinfo for llvm::raw_ostream"
# at link time.
#
# **Default ON** — every build (Debug, Release, dev container,
# CI, agent runs) ships with ASan unless explicitly disabled via
# `-DNSL_ENABLE_ASAN=OFF`. M9 release builds opt out; everything
# upstream stays leak-checked. Catches leaks before they manifest
# as OOM kills under repeated test runs.
option(NSL_ENABLE_ASAN  "Build with AddressSanitizer + LeakSanitizer" ON)

if(NSL_ENABLE_ASAN)
  message(STATUS "NSL_ENABLE_ASAN=ON: building with -fsanitize=address (LSan via ASAN_OPTIONS)")
  add_compile_options(-fsanitize=address -fno-omit-frame-pointer)
  add_link_options(-fsanitize=address)
endif()

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

# LLVM/MLIR ship CMake helper modules (AddLLVM, TableGen, AddMLIR)
# that are NOT auto-included by find_package(... CONFIG). The
# `add_lit_testsuite` command in test/CMakeLists.txt and the future
# TableGen/MLIR-dialect helpers used by nsl-dialect (M4+) need them
# on CMAKE_MODULE_PATH and explicitly included.
list(APPEND CMAKE_MODULE_PATH "${LLVM_CMAKE_DIR}")
list(APPEND CMAKE_MODULE_PATH "${MLIR_CMAKE_DIR}")

# AddLLVM resolves lit via LLVM_EXTERNAL_LIT. The vendored LLVM build
# bakes in a path under /opt/llvm/bin/llvm-lit that is NOT actually
# installed (LLVM_INSTALL_UTILS=ON installs the C++ helpers but not
# the Python lit module). Detect lit on PATH and override before
# AddLLVM consumes the cached default.
find_program(NSL_LIT_PATH NAMES llvm-lit lit
  HINTS ${LLVM_TOOLS_BINARY_DIR})
if(NSL_LIT_PATH)
  set(LLVM_EXTERNAL_LIT "${NSL_LIT_PATH}" CACHE FILEPATH
      "Path to lit; auto-detected by NSL_LIT_PATH" FORCE)
  message(STATUS "Using lit at ${LLVM_EXTERNAL_LIT}")
else()
  message(WARNING
    "lit not found on PATH or in LLVM_TOOLS_BINARY_DIR. The check-nslc "
    "target will be unavailable. Install via `pip install lit` or apt.")
endif()

include(TableGen)
include(AddLLVM)
include(AddMLIR)

# LLVM/MLIR/CIRCT ship CMake `IMPORTED` targets (LLVMSupport,
# MLIRSupport, CIRCTHW, …) but their `*Config.cmake` files do NOT
# populate `INTERFACE_INCLUDE_DIRECTORIES` on the targets themselves
# — the LLVM-ecosystem convention is for consumers to pull
# `${LLVM_INCLUDE_DIRS}` / `${MLIR_INCLUDE_DIRS}` /
# `${CIRCT_INCLUDE_DIRS}` separately. Without this, downstream
# `target_link_libraries(... PUBLIC LLVMSupport)` propagates the
# library link but not the header search path, and any TU that
# `#include <llvm/...>` / `<mlir/...>` / `<circt/...>` fails at
# compile time. SYSTEM marks the headers as system includes so
# warnings inside them don't fire. CIRCT was added at M4 to support
# `tools/nsl-opt/main.cpp`'s registration of the 5 CIRCT dialects
# per design §11 lines 1146–1150 and spec FR-014.
include_directories(SYSTEM ${MLIR_INCLUDE_DIRS} ${LLVM_INCLUDE_DIRS} ${CIRCT_INCLUDE_DIRS})

# -----------------------------------------------------------------------------
# Subdirectories
# -----------------------------------------------------------------------------

add_subdirectory(lib)

# T-track tooling library (`libNSLLSP.a`) — outside the §3 layer
# table that `lib/CMakeLists.txt` aggregates via `add_nsl_library`.
# Per Constitution Principle II §3, T-track tools (`nsl-lsp`,
# `nsl-fmt`, `nsl-lint`) reuse `libNSLFrontend.a` rather than
# duplicate frontend layers. Wired here from the top-level so
# `lib/CMakeLists.txt` retains its documented "aggregator for the
# nine compiler-track layer libraries" scope.
add_subdirectory(lib/LSP)

add_subdirectory(tools)

if(NSL_BUILD_TESTS)
  enable_testing()
  add_subdirectory(test)
  add_subdirectory(test_unit)
endif()
