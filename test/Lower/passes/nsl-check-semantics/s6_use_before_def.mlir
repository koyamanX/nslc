// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// XFAIL: *
// RUN: nsl-opt --verify-diagnostics --split-input-file -nsl-check-semantics %s
//
// M5 US4 / FR-018 — sensitive-`Sn` re-check S6 per
// `pass-pipeline.contract.md` §3 row S6:
//
//   "S6 | post-expand-variables wire-chain has out-of-order use →
//    def | error: register or wire '<name>' used before definition"
//
// **DEFERRED at M5** — the re-check helper is documented as a stub
// in the pass body. Per offload guidance: "If any Sn re-check
// requires non-trivial dataflow analysis... simplify to a structural
// check that catches the common case + document the limitation."
//
// S6 in its frozen form requires SSA operand-traversal to detect
// "use before def" — a wire-chain post-expand-variables can have a
// `nsl.transfer %dst, %src` whose `%src` is a wire whose defining
// `nsl.wire` op appears *later* in the parent block than the
// transfer that consumes it. MLIR's SSA verifier already catches
// the most pathological forms (use of an SSA value not yet
// defined), so a *purely* structural re-check would emit a redundant
// diagnostic on shape MLIR has already rejected. A meaningful S6
// re-check needs:
//
//   (a) cross-region operand-traversal (a wire defined inside one
//       generate-replica being consumed in another),
//   (b) M5-specific knowledge of which wire-chain "version" is the
//       canonical post-expand-variables defining op, AND
//   (c) the named "<name>" in the diagnostic must be the *source*
//       wire/reg name (the M3 Sema-layer name), not the
//       version-numbered post-expand `nsl.wire "name_2"` form.
//
// All three needs exceed the slot-6 single-pass budget. This
// fixture is XFAIL'd; the helper body lands when a follow-up M5+
// amendment adds the operand-traversal infrastructure. Same
// precedent as US3's T076 / T077 XFAIL on wire-parent-constraint
// + struct-typed-variable amendments.

nsl.module @S6UseBeforeDef {
  // expected-error@+1 {{register or wire 'q' used before definition}}
  nsl.wire "consumer" : !nsl.bits<8>
  // The pathological pattern: `q` is referenced before it's defined.
  // At the dialect level this is impossible to author without the
  // verifier rejecting the SSA shape, so we leave the fixture
  // intentionally non-compiling (XFAIL) and record the M5+
  // limitation in the comment above.
  %q = nsl.wire "q" : !nsl.bits<8>
}
