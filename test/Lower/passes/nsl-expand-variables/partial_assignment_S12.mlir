// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// RUN: nsl-opt -nsl-expand-variables %s | FileCheck %s
//
// M5 US3 / FR-015 — `NSLExpandVariablesPass` (slot 3). Acceptance
// scenario 3 (`spec.md:283`):
//
//   "Given a variable with partial-assignment v[3:0] = x;
//    v[7:4] = y; (S12 permits this on variables), when the pass
//    runs, then the post-pass IR contains a wire-chain whose final
//    SSA value is the slice-merge of x and y per the bit-positions
//    assigned."
//
// The `nsl.variable` op only accepts whole-width writes (the
// transfer's `SameTypeOperands` trait + ZeroResults shape forbids
// a slice-LHS dst). The visitor (T082, future) lowers `v[3:0] = x`
// as `transfer dst=%v, src=concat(<upper-half>, x)` — i.e., the
// slice-merge is constructed in the src expression so the dst
// remains the whole variable.
//
// **First-write modelling note**: at the very first partial-write
// site, NSL semantics let the not-assigned bits hold "undef" / "x"
// (S12). At the dialect layer we model this by initialising those
// bits to a constant zero — the visitor synthesises a `concat`
// whose unread-half is `nsl.constant 0`. This sidesteps the
// "reads-before-any-write" pattern that S6 generally rejects, and
// keeps the pass-standalone semantics local: each transfer is
// whole-width and self-contained.
//
// At the SECOND partial-write, the not-assigned half is taken
// from the just-written prior version — that's where the wire-
// chain semantics shine. The pass remaps the read of `%v` (in
// the second concat's operand) to the first write's wire result.
//
// CHECK-LABEL: nsl.module @PartialAssign
// CHECK-NOT: nsl.variable
nsl.module @PartialAssign {
  // CHECK: nsl.wire "x" : !nsl.bits<4>
  %x = nsl.wire "x" : !nsl.bits<4>
  // CHECK: nsl.wire "y" : !nsl.bits<4>
  %y = nsl.wire "y" : !nsl.bits<4>
  // CHECK: nsl.wire "result" : !nsl.bits<8>
  %result = nsl.wire "result" : !nsl.bits<8>
  %zero4 = nsl.constant 0 : !nsl.bits<4>
  %v = nsl.variable "v" : !nsl.bits<8>
  // First partial-assign: v[3:0] = x. Visitor models the unread
  // upper half as constant zero (S12 first-write undef → 0).
  // Post-pass: dst becomes wire-version "v".
  // CHECK: %[[V0:.*]] = nsl.wire "v" : !nsl.bits<8>
  %merged0 = nsl.concat %zero4, %x : (!nsl.bits<4>, !nsl.bits<4>) -> !nsl.bits<8>
  // CHECK: nsl.transfer %[[V0]], %{{.*}} : !nsl.bits<8>
  nsl.transfer %v, %merged0 : !nsl.bits<8>
  // Second partial-assign: v[7:4] = y. Read of %v's lower half
  // remaps to the prior wire version (V0).
  // CHECK: %[[V1:.*]] = nsl.wire "v_1" : !nsl.bits<8>
  %lower1 = nsl.extract %v, 0 : !nsl.bits<8> -> !nsl.bits<4>
  %merged1 = nsl.concat %y, %lower1 : (!nsl.bits<4>, !nsl.bits<4>) -> !nsl.bits<8>
  // CHECK: nsl.transfer %[[V1]], %{{.*}} : !nsl.bits<8>
  nsl.transfer %v, %merged1 : !nsl.bits<8>
  // Final read: result = v → consumes V1 (the second version).
  // CHECK: nsl.transfer %{{.*}}, %[[V1]] : !nsl.bits<8>
  nsl.transfer %result, %v : !nsl.bits<8>
}
