// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// RUN: nsl-opt -nsl-expand-variables %s | FileCheck %s
//
// M5 US3 / FR-015 — `NSLExpandVariablesPass` (slot 3). Acceptance
// scenario 2 (`spec.md:282`):
//
//   "Given the same module with a chain of three writes
//    (v = a; v = v + 1; v = v * 2;) followed by one read, when the
//    pass runs, then the post-pass IR contains three distinct
//    wire-version SSA values AND the read consumes the third
//    version."
//
// Each write produces a fresh `nsl.wire` SSA value. Subsequent
// reads (operand position) of the variable are remapped to the
// most recently written wire version. The final read sees the
// third version.

// CHECK-LABEL: nsl.module @ScalarChain3
// CHECK-NOT: nsl.variable
nsl.module @ScalarChain3 {
  // CHECK: nsl.wire "a" : !nsl.bits<8>
  %a = nsl.wire "a" : !nsl.bits<8>
  // CHECK: nsl.wire "b" : !nsl.bits<8>
  %b = nsl.wire "b" : !nsl.bits<8>
  // Three wire-versions of v emerge.
  // CHECK: %[[V0:.*]] = nsl.wire "v" : !nsl.bits<8>
  // CHECK: nsl.transfer %[[V0]], %{{.*}} : !nsl.bits<8>
  // CHECK: %[[V1:.*]] = nsl.wire "v_1" : !nsl.bits<8>
  // CHECK: nsl.transfer %[[V1]], %{{.*}} : !nsl.bits<8>
  // CHECK: %[[V2:.*]] = nsl.wire "v_2" : !nsl.bits<8>
  // CHECK: nsl.transfer %[[V2]], %{{.*}} : !nsl.bits<8>
  %v = nsl.variable "v" : !nsl.bits<8>
  %one = nsl.constant 1 : !nsl.bits<8>
  %two = nsl.constant 2 : !nsl.bits<8>
  // Write 1: v = a
  nsl.transfer %v, %a : !nsl.bits<8>
  // Write 2: v = v + 1 — the RHS read of %v is remapped to V0.
  %t1 = nsl.add %v, %one : !nsl.bits<8>
  nsl.transfer %v, %t1 : !nsl.bits<8>
  // Write 3: v = v * 2 — RHS read of %v remapped to V1.
  %t2 = nsl.mul %v, %two : !nsl.bits<8>
  nsl.transfer %v, %t2 : !nsl.bits<8>
  // Final read: b = v — consumes the third version (V2).
  // CHECK: nsl.transfer %{{.*}}, %[[V2]] : !nsl.bits<8>
  nsl.transfer %b, %v : !nsl.bits<8>
}
