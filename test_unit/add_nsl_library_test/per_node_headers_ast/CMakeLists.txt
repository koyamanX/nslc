# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#
# add_nsl_library_test/per_node_headers_ast — exercises contract row 7.
# nsl-ast accepts per-node-kind headers (Principle II's AST per-node
# headers exception).

cmake_minimum_required(VERSION 3.22)
project(test_per_node_headers_ast LANGUAGES CXX)

list(APPEND CMAKE_MODULE_PATH "${NSLC_CMAKE_DIR}")
include(NSLDeterminism)
include(AddNSLLibrary)

add_nsl_library(nsl-ast
  HEADERS
    include/nsl/AST/Decl.h
    include/nsl/AST/Stmt.h
    include/nsl/AST/Expr.h)
