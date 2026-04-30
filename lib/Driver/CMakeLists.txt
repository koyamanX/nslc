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
  Compilation.cpp
  LowerToNSL.cpp
  RunNSLPasses.cpp
  HEADERS
    ${CMAKE_SOURCE_DIR}/include/nsl/Driver/EmitTokens.h
    ${CMAKE_SOURCE_DIR}/include/nsl/Driver/EmitAST.h
    ${CMAKE_SOURCE_DIR}/include/nsl/Driver/Sema.h
    ${CMAKE_SOURCE_DIR}/include/nsl/Driver/Compilation.h
  DEPENDS
    nsl-basic
    nsl-preprocess
    nsl-lex
    nsl-ast
    nsl-parse
    nsl-sema
    # M4 (FR-004 / FR-019): nsl-driver gains a build-time edge into
    # nsl-dialect so the `Compilation` ctor can call
    # `mlirCtx_.loadDialect<nsl::dialect::NSLDialect>()` per design
    # §11 line 1145, and so `LowerToNSL.cpp` / `RunNSLPasses.cpp`
    # can reference dialect types in their forward-looking M5
    # bodies. The reverse edge (nsl-dialect → nsl-driver) is
    # forbidden by Principle II + AddNSLLibrary's layer-index check.
    nsl-dialect
    nsl-lower)
