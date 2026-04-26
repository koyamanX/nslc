# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#
# add_nsl_library_test/unknown_layer_name — exercises contract row 2.
# `add_nsl_library(nsl-bogus)` MUST FATAL_ERROR with a "§3 layer
# table" diagnostic. CTest WILL_FAIL inverts the expected exit code.

cmake_minimum_required(VERSION 3.22)
project(test_unknown_layer_name LANGUAGES CXX)

list(APPEND CMAKE_MODULE_PATH "${NSLC_CMAKE_DIR}")
include(NSLDeterminism)
include(AddNSLLibrary)

add_nsl_library(nsl-bogus)
