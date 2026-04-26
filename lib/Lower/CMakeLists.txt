# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#
# nsl-lower — layer 8. Two lowering pipelines: AST → nsl::* dialect
# (M5 structural-expansion passes) and nsl::* → CIRCT (M6 hw / comb /
# seq / sv emission). M0 skeleton; real passes at M5/M6.

add_nsl_library(nsl-lower
  DEPENDS nsl-sema nsl-dialect
  LINK_LIBS
    CIRCTHW
    CIRCTComb
    CIRCTSeq
    CIRCTSV)
