# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#
# nsl-driver — layer 9. CLI orchestration consumed by tools/nslc.
# Aggregates every layer below; the top-level `nslc` binary links
# only against nsl-driver and inherits everything transitively
# (Principle II).
#
# At M1 (Phase 3, US1) nsl-driver gains the `-emit=tokens` code path
# in `EmitTokens.cpp`; the preprocessor link sits in DEPENDS even
# though Phase 3 doesn't exercise it, because Phase 4 (T061) extends
# `EmitTokens.cpp` with the preprocess pre-pass.

add_nsl_library(nsl-driver
  EmitTokens.cpp
  HEADERS
    ${CMAKE_SOURCE_DIR}/include/nsl/Driver/EmitTokens.h
  DEPENDS
    nsl-basic
    nsl-preprocess
    nsl-lex
    nsl-ast
    nsl-parse
    nsl-sema
    nsl-dialect
    nsl-lower)
