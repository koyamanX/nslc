# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#
# add_nsl_library_test/downward_dep — exercises contract row 3.
# `nsl-lex DEPENDS nsl-basic` is a strictly-lower-layer dep and MUST
# configure cleanly.

cmake_minimum_required(VERSION 3.22)
project(test_downward_dep LANGUAGES CXX)

list(APPEND CMAKE_MODULE_PATH "${NSLC_CMAKE_DIR}")
include(NSLDeterminism)
include(AddNSLLibrary)

add_nsl_library(nsl-basic)
add_nsl_library(nsl-lex DEPENDS nsl-basic)
