// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// RUN: nsl-opt --verify-diagnostics --split-input-file -nsl-check-semantics %s
//
// M5 US4 / FR-018 — `NSLCheckSemanticsPass` (slot 6) residue
// detection. Acceptance scenario 1 (`spec.md` US4):
//
//   "Given an `nsl.module` containing `nsl.reg "buf_%TYPO%" :
//    !nsl.bits<8>`, when NSLCheckSemanticsPass runs, then the pass
//    emits the FROZEN diagnostic
//    `error: unresolved macro splice '%TYPO%' after structural
//    expansion` AND signals failure."
//
// Frozen regex (`residue-detection.contract.md` §2):
//
//   R"((%[A-Za-z_][A-Za-z0-9_]*%))"
//
// Frozen diagnostic format (`residue-detection.contract.md` §4):
//
//   error: unresolved macro splice '%<IDENT>%' after structural expansion
//
// `%TYPO%` matches `[A-Za-z_][A-Za-z0-9_]*` so the regex fires.

nsl.module @ResidueTypo {
  // expected-error@+1 {{unresolved macro splice '%TYPO%' after structural expansion}}
  nsl.reg "buf_%TYPO%" : !nsl.bits<8>
}
