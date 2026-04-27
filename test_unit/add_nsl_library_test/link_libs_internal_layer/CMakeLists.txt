# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#
# add_nsl_library_test/link_libs_internal_layer — exercises the
# `LINK_LIBS rejects internal nsl-* layers` guard added to
# cmake/AddNSLLibrary.cmake.
#
# `nsl-basic` is an internal nsl-layer; placing it in LINK_LIBS
# would bypass the dependency-direction guard (Principle II) that
# DEPENDS enforces. The macro MUST FATAL_ERROR with a clear
# "use DEPENDS instead" diagnostic.

cmake_minimum_required(VERSION 3.22)
project(test_link_libs_internal_layer LANGUAGES CXX)

list(APPEND CMAKE_MODULE_PATH "${NSLC_CMAKE_DIR}")
include(NSLDeterminism)
include(AddNSLLibrary)

add_nsl_library(nsl-basic)
add_nsl_library(nsl-lex LINK_LIBS nsl-basic)
