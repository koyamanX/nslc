# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#
# nsl-sema — layer 6. Semantic analysis (S1..S29 constraints from
# docs/spec/nsl_lang.ebnf). M0 skeleton; per-Sn one-pass-and-one-fail
# fixtures + implementation lands at M3.

add_nsl_library(nsl-sema
  DEPENDS nsl-ast)
