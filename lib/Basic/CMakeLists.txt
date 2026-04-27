# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#
# nsl-basic — layer 1. Foundation: SourceLocation, Diagnostic, and
# the small primitives every later layer depends on. M0 ships an
# empty skeleton (the macro auto-generates an anchor TU); real types
# arrive at M1 alongside the lexer.

add_nsl_library(nsl-basic)
