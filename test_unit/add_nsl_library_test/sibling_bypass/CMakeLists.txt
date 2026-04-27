# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#
# add_nsl_library_test/sibling_bypass — exercises contract row 5.
# `nsl-lex DEPENDS nsl-parse` is upward (lex layer 3 < parse layer 5)
# AND would create a cycle (parse already depends on lex). The macro
# MUST FATAL_ERROR.

cmake_minimum_required(VERSION 3.22)
project(test_sibling_bypass LANGUAGES CXX)

list(APPEND CMAKE_MODULE_PATH "${NSLC_CMAKE_DIR}")
include(NSLDeterminism)
include(AddNSLLibrary)

add_nsl_library(nsl-parse)
add_nsl_library(nsl-lex DEPENDS nsl-parse)
