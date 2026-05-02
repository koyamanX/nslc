// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// RUN: nsl-opt --verify-diagnostics --split-input-file -nsl-check-semantics %s
//
// M5 US4 / FR-018 — second residue case from
// `residue-detection.contract.md` §8 row 2: a `StringAttr` whose
// entire value is a residue token (`%UNDEFINED%`) — covers the
// "no surrounding text" edge case (the regex MUST still match).

nsl.module @ResidueUndefined {
  // expected-error@+1 {{unresolved macro splice '%UNDEFINED%' after structural expansion}}
  nsl.reg "%UNDEFINED%" : !nsl.bits<8>
}
