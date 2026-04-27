# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#
# add_nsl_library_test/multi_header_basic — exercises contract row 6.
# nsl-basic accepts both SourceLocation.h and Diagnostic.h in HEADERS
# (Principle II's nsl-basic multi-header exception).

cmake_minimum_required(VERSION 3.22)
project(test_multi_header_basic LANGUAGES CXX)

list(APPEND CMAKE_MODULE_PATH "${NSLC_CMAKE_DIR}")
include(NSLDeterminism)
include(AddNSLLibrary)

add_nsl_library(nsl-basic
  HEADERS
    include/nsl/Basic/SourceLocation.h
    include/nsl/Basic/Diagnostic.h)
