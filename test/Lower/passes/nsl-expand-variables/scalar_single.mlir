// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// RUN: nsl-opt -nsl-expand-variables %s | FileCheck %s
//
// M5 US3 / FR-015 — `NSLExpandVariablesPass` (slot 3). Acceptance
// scenario 1 (`spec.md:281`):
//
//   "Given an nsl.module containing nsl.variable 'v' : !nsl.bits<8>
//    with a single transfer-write v = a and a single read b = v,
//    when NSLExpandVariablesPass runs, then the post-pass IR
//    contains zero nsl.variable AND a single nsl.wire 'v' :
//    !nsl.bits<8> AND the read's source becomes the wire's
//    defining op."
//
// Transformation rule per FR-015: replace each `nsl.variable` with
// an SSA chain of `nsl.wire` declarations and `nsl.transfer` ops.
// One write produces one wire-version. The read use of the
// variable's result is replaced with the latest wire's result.

// CHECK-LABEL: nsl.module @ScalarSingle
// CHECK-NOT: nsl.variable
nsl.module @ScalarSingle {
  // Source operand for the transfer-write.
  // CHECK: nsl.wire "a" : !nsl.bits<8>
  %a = nsl.wire "a" : !nsl.bits<8>
  // Destination wire for the read.
  // CHECK: nsl.wire "b" : !nsl.bits<8>
  %b = nsl.wire "b" : !nsl.bits<8>
  // The variable disappears; per scenario 1, a single wire named
  // "v" replaces it (one-write chain has length 1).
  // CHECK: %[[V0:.*]] = nsl.wire "v" : !nsl.bits<8>
  %v = nsl.variable "v" : !nsl.bits<8>
  // Write: v = a → transfer dst=wire "v", src=%a.
  // CHECK: nsl.transfer %[[V0]], %{{.*}} : !nsl.bits<8>
  nsl.transfer %v, %a : !nsl.bits<8>
  // Read: b = v → the read's source becomes the wire's result.
  // CHECK: nsl.transfer %{{.*}}, %[[V0]] : !nsl.bits<8>
  nsl.transfer %b, %v : !nsl.bits<8>
}
