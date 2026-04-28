# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#
# nsl-driver — layer 9. CLI orchestration consumed by tools/nslc.
# Aggregates every layer below; the top-level `nslc` binary links
# only against nsl-driver and inherits everything transitively
# (Principle II).
#
# At M2 (Phase 3) nsl-driver gains the `-emit=ast` code path in
# `EmitAST.cpp` alongside M1's `EmitTokens.cpp`.

add_nsl_library(nsl-driver
  EmitTokens.cpp
  EmitAST.cpp
  Sema.cpp
  HEADERS
    ${CMAKE_SOURCE_DIR}/include/nsl/Driver/EmitTokens.h
    ${CMAKE_SOURCE_DIR}/include/nsl/Driver/EmitAST.h
    ${CMAKE_SOURCE_DIR}/include/nsl/Driver/Sema.h
  DEPENDS
    nsl-basic
    nsl-preprocess
    nsl-lex
    nsl-ast
    nsl-parse
    nsl-sema
    nsl-dialect
    nsl-lower)
