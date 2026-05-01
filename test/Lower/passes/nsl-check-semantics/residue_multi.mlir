// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// RUN: nsl-opt --verify-diagnostics --split-input-file -nsl-check-semantics %s
//
// M5 US4 / FR-018 — multi-match case from US4 acceptance scenario 3
// + `residue-detection.contract.md` §8 row 3. Two distinct residue
// tokens on two separate ops MUST yield two diagnostics; one
// StringAttr containing two tokens MUST yield two diagnostics
// (multi-error recovery within a single attribute scan, frozen by
// `residue-detection.contract.md` §2 last paragraph).

nsl.module @ResidueMulti {
  // Two tokens on the SAME StringAttr — `std::regex_iterator` finds
  // both non-overlapping matches.
  // expected-error@+2 {{unresolved macro splice '%X%' after structural expansion}}
  // expected-error@+1 {{unresolved macro splice '%Y%' after structural expansion}}
  nsl.reg "%X%_%Y%" : !nsl.bits<8>
  // Tokens on DISTINCT ops — independent diagnostics.
  // expected-error@+1 {{unresolved macro splice '%FOO%' after structural expansion}}
  nsl.wire "buf_%FOO%" : !nsl.bits<8>
  // expected-error@+1 {{unresolved macro splice '%BAR%' after structural expansion}}
  nsl.wire "buf_%BAR%" : !nsl.bits<8>
}
