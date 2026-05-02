// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// RUN: nsl-opt --verify-diagnostics --split-input-file -nsl-check-semantics %s
//
// M5 US4 / FR-018 — sensitive-`Sn` re-check S15 per
// `pass-pipeline.contract.md` §3 row S15:
//
//   "S15 | bit-slice index resolves to non-compile-time-constant
//    after slot 1 | error: bit-slice index is non-constant after
//    parameter resolution"
//
// **VACUOUS ON M5 SURFACE — converted to a no-violation PASS case.**
// At M5's frozen 79-op dialect surface, `nsl.slice` (and the
// closely-related `nsl.extract`) carry `I64Attr` lo/hi indices —
// not operand-side SSA Values referencing `nsl.constant` or an
// unresolved param ref. The "non-constant after slot 1" condition
// is therefore unreachable on PURE-NSL inputs at M5; the re-check
// helper in `NSLCheckSemanticsPass` is a documented no-op stub for
// S15 on M5.
//
// When a future M4+ amendment introduces a bit-slice op variant
// whose index slot is operand-side `Value` (rather than `I64Attr`),
// this fixture pivots back into a fail-case shape: the helper
// walks every slice op, asserts each index Value is defined by an
// `nsl.constant`, and emits the FROZEN diagnostic for any that
// isn't. Until that amendment lands, this fixture asserts the
// pass accepts the structurally-clean shape without a (vacuously-
// impossible-to-trigger) diagnostic.

nsl.module @S15PostParam {
  // No bit-slice present: structurally clean. Even if there were
  // an `nsl.extract` op here, its `lo` is an I64Attr — by
  // construction a compile-time constant — so S15 cannot fire.
  nsl.wire "src" : !nsl.bits<32>
}
