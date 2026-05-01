// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// XFAIL: *
// RUN: nsl-opt -nsl-expand-variables %s | FileCheck %s
//
// M5 US3 / FR-015 — `NSLExpandVariablesPass` (slot 3). Acceptance
// scenario 4 (`spec.md:284`):
//
//   "Given a struct-typed variable variable s : @T; with s.a = x;
//    s.b = y;, when the pass runs, then the post-pass IR has the
//    chain operating per-field (the .a field sees one transfer,
//    the .b field sees one transfer; reads of s materialize via
//    field-level wires)."
//
// **DEFERRED — M4 op-surface gap**. `nsl.variable`'s result type
// is constrained to `NSL_AnyBits` only (NSLOps.td:280, FR-013) —
// the dialect verifier rejects `!nsl.struct<@T>` outright at parse
// time. The spec's "struct-SSA-split" path requires either:
//   (a) a fifth M4 amendment relaxing `nsl.variable` to
//       `NSL_BitsOrStruct` (would also require an M5 visitor that
//       knows to decompose at the AST layer), OR
//   (b) AST-time per-field decomposition: `variable s : @T;` with
//       fields `a` and `b` lowers to two separate `nsl.variable`
//       ops `s_a : !nsl.bits<Wa>` and `s_b : !nsl.bits<Wb>`. The
//       pass then sees only scalars; option (b) is the documented
//       implementer choice (smaller surface).
//
// Both options require visitor work (T082) plus the StructDecl /
// field-table infrastructure that does not exist at M5 today. The
// per-pass behavioural shape is deferred until that infrastructure
// lands. Until then this fixture documents the intent and is
// XFAIL (the IR does not even parse — `nsl.variable` rejects
// !nsl.struct<@T>).
//
// US2 T066/T067 set the precedent for this deferral pattern.

nsl.module @StructTyped {
  // Intent: a struct-typed variable would be authored as
  //
  //   %s = nsl.variable "s" : !nsl.struct<@T>
  //
  // Per option (b) above, the visitor would instead emit:
  //
  //   %s_a = nsl.variable "s_a" : !nsl.bits<Wa>
  //   %s_b = nsl.variable "s_b" : !nsl.bits<Wb>
  //
  // The pass then expands each scalar variable into its own
  // wire-chain independently. That matches the spec's "the .a
  // field sees one transfer, the .b field sees one transfer;
  // reads of s materialize via field-level wires."
  //
  // Pre-XFAIL placeholder — the line below is what would fail to
  // parse against current M4 verifier.
  %s = nsl.variable "s" : !nsl.bits<16>
  // CHECK-NOT: nsl.variable
}
