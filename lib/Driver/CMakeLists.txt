# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#
# nsl-driver — layer 9. CLI orchestration consumed by tools/nslc.
# Aggregates every layer below; the top-level `nslc` binary links
# only against nsl-driver and inherits everything transitively
# (Principle II).
#
# At M0 nsl-driver carries no real behavior — `nslc --version` reads
# the configure-time-generated header `nsl/Driver/Version.h` (which
# nsl-driver's PUBLIC include directories make visible to consumers).
# Real `-emit=*` flags arrive milestone-by-milestone from M1 onward.

add_nsl_library(nsl-driver
  DEPENDS
    nsl-basic
    nsl-preprocess
    nsl-lex
    nsl-ast
    nsl-parse
    nsl-sema
    nsl-dialect
    nsl-lower)
