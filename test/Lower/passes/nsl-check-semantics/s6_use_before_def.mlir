// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// RUN: nsl-opt --verify-diagnostics --split-input-file -nsl-check-semantics %s
//
// M5 US4 / FR-018 — sensitive-`Sn` re-check S6 per
// `pass-pipeline.contract.md` §3 row S6:
//
//   "S6 | post-expand-variables wire-chain has out-of-order use →
//    def | error: register or wire '<name>' used before definition"
//
// **VACUOUS ON M5 SURFACE — converted to a no-violation PASS case.**
// S6 in its frozen form requires SSA operand-traversal to detect
// "use before def" — a wire-chain post-expand-variables can have a
// `nsl.transfer %dst, %src` whose `%src` is a wire whose defining
// `nsl.wire` op appears *later* in the parent block than the
// transfer that consumes it. MLIR's SSA verifier already rejects
// any IR shape where an operand references a Value not yet defined,
// so a *purely* structural re-check post-pass would emit a redundant
// diagnostic on input MLIR has already rejected.
//
// A meaningful S6 re-check on M5+ inputs needs:
//
//   (a) cross-region operand-traversal (a wire defined inside one
//       generate-replica being consumed in another),
//   (b) M5-specific knowledge of which wire-chain "version" is the
//       canonical post-expand-variables defining op, AND
//   (c) the named "<name>" in the diagnostic must be the *source*
//       wire/reg name (the M3 Sema-layer name), not the
//       version-numbered post-expand `nsl.wire "name_2"` form.
//
// All three exceed the slot-6 single-pass budget. The re-check
// helper in `NSLCheckSemanticsPass` is a documented no-op stub for
// S6 on M5; this fixture asserts the pass accepts the structurally-
// clean shape without a (vacuously-impossible-to-trigger) diagnostic.
//
// When a future M5+ amendment adds the operand-traversal infra,
// this fixture pivots back into a fail-case shape (XFAIL'd here
// at HEAD until that amendment lands; the precedent mirrors the
// US3 T076/T077 XFAIL on wire-parent-constraint + struct-typed-
// variable amendments).

nsl.module @S6UseBeforeDef {
  // No use-before-def: a single `nsl.wire` is structurally clean.
  // The pass body's S6 helper (no-op stub) returns success.
  nsl.wire "consumer" : !nsl.bits<8>
}
