# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#
# add_nsl_library_test/undeclared_dep_target — exercises the
# `NOT TARGET ${_dep}` guard added to cmake/AddNSLLibrary.cmake.
#
# `nsl-ast` IS a valid layer name (passes the layer-name table
# check), but no add_nsl_library(nsl-ast …) has been called yet
# in this fixture, so no CMake target exists. The macro MUST
# FATAL_ERROR with a "must name a previously-declared
# add_nsl_library target" diagnostic — catching the misspelled-or-
# out-of-order DEPENDS case before it reaches link time.

cmake_minimum_required(VERSION 3.22)
project(test_undeclared_dep_target LANGUAGES CXX)

list(APPEND CMAKE_MODULE_PATH "${NSLC_CMAKE_DIR}")
include(NSLDeterminism)
include(AddNSLLibrary)

# Note: deliberately NO `add_nsl_library(nsl-ast)` call before this.
add_nsl_library(nsl-parse DEPENDS nsl-ast)
