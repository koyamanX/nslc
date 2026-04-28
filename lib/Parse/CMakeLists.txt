# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#
# nsl-parse — layer 5. Recursive-descent parser building nsl-ast
# nodes from the nsl-lex token stream. M2 implements the parser
# proper (M0 ship was a skeleton; the Phase 2 PrecedenceTable.h
# private header is consumed by ParseExpr.cpp).
#
# Per `cmake/AddNSLLibrary.cmake`, the macro auto-validates layer-
# direction (Principle II downward-only flow) on `DEPENDS`.
# `nsl-parse` MUST NOT depend on `nsl-sema` or any later layer
# (FR-003 / SC-009).

add_nsl_library(nsl-parse
  Parser.cpp
  ParseDecl.cpp
  ParseStmt.cpp
  ParseExpr.cpp
  HEADERS
    ${CMAKE_SOURCE_DIR}/include/nsl/Parse/Parser.h
  DEPENDS
    nsl-basic
    nsl-lex
    nsl-ast)
