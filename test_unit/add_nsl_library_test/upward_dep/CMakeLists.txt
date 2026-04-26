# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#
# add_nsl_library_test/upward_dep — exercises contract row 4.
# `nsl-basic DEPENDS nsl-lex` violates Principle II (basic at layer 1
# cannot depend on lex at layer 3). The macro MUST FATAL_ERROR with
# an "index M ≥ N" diagnostic citing nsl_compiler_design.md §3.

cmake_minimum_required(VERSION 3.22)
project(test_upward_dep LANGUAGES CXX)

list(APPEND CMAKE_MODULE_PATH "${NSLC_CMAKE_DIR}")
include(NSLDeterminism)
include(AddNSLLibrary)

add_nsl_library(nsl-lex)
add_nsl_library(nsl-basic DEPENDS nsl-lex)
