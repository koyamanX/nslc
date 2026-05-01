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
// transfer's `SameTypeOperands` trait + ZeroResults shape forbids a
// slice-LHS dst). The visitor (T082, future) lowers `v[3:0] = x` as
// `transfer dst=%v, src=concat(<upper-half-of-old-v>, x)` — i.e.,
// the slice-merge is constructed in the src expression so the dst
// remains the whole variable. From the pass-standalone POV, that
// reads as: each transfer is whole-width, and the chain of
// versions propagates the slice-merge naturally — the pass clones
// the `concat`/`extract` operand sub-DAG verbatim and only renames
// the dst to the per-version wire.
//
// The fixture below mirrors the post-visitor IR shape: two
// whole-width transfers whose src is a concat of x/y with the
// preserved upper/lower half of the prior version.

// CHECK-LABEL: nsl.module @PartialAssign
// CHECK-NOT: nsl.variable
nsl.module @PartialAssign {
  // CHECK: nsl.wire "x" : !nsl.bits<4>
  %x = nsl.wire "x" : !nsl.bits<4>
  // CHECK: nsl.wire "y" : !nsl.bits<4>
  %y = nsl.wire "y" : !nsl.bits<4>
  // CHECK: nsl.wire "result" : !nsl.bits<8>
  %result = nsl.wire "result" : !nsl.bits<8>
  // CHECK: %[[V0:.*]] = nsl.wire "v" : !nsl.bits<8>
  // CHECK: %[[V1:.*]] = nsl.wire "v_1" : !nsl.bits<8>
  %v = nsl.variable "v" : !nsl.bits<8>
  // v[3:0] = x → src builds {<upper-of-prior-v>, x}; dst is %v
  // (post-pass: dst becomes V0).
  %upper0 = nsl.extract %v, 4 : !nsl.bits<8> -> !nsl.bits<4>
  %merged0 = nsl.concat %upper0, %x : (!nsl.bits<4>, !nsl.bits<4>) -> !nsl.bits<8>
  nsl.transfer %v, %merged0 : !nsl.bits<8>
  // v[7:4] = y → src builds {y, <lower-of-prior-v>}; the read of
  // %v on the operand of `nsl.extract` is remapped to V0. The
  // transfer's dst is remapped to V1.
  %lower1 = nsl.extract %v, 0 : !nsl.bits<8> -> !nsl.bits<4>
  %merged1 = nsl.concat %y, %lower1 : (!nsl.bits<4>, !nsl.bits<4>) -> !nsl.bits<8>
  nsl.transfer %v, %merged1 : !nsl.bits<8>
  // Final read: result = v → consumes V1 (the second version).
  // CHECK: nsl.transfer %{{.*}}, %[[V1]] : !nsl.bits<8>
  nsl.transfer %result, %v : !nsl.bits<8>
}
