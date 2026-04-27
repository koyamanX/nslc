# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#
# add_nsl_library_test/valid_layer_name — exercises contract row 1.
# `add_nsl_library(nsl-basic)` MUST configure cleanly.

cmake_minimum_required(VERSION 3.22)
project(test_valid_layer_name LANGUAGES CXX)

list(APPEND CMAKE_MODULE_PATH "${NSLC_CMAKE_DIR}")
include(NSLDeterminism)
include(AddNSLLibrary)

add_nsl_library(nsl-basic)
