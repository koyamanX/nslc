// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// RUN: nsl-opt %s | FileCheck %s
// RUN: nsl-opt %s | nsl-opt - | FileCheck %s
//
// FR-010 round-trip for `nsl.connect` (design §7 line 889). Per
// Q3 Option A, operand types are checked under strict `mlir::Type`
// equality — both operands here are `!nsl.bits<8>`, no implicit
// bits<->struct cast. (M5 lowering inserts `nsl.struct_cast` at any
// user-written cast site; the dialect verifier never tolerates
// implicit reinterpretation.)

// CHECK-LABEL: nsl.module @Wired
nsl.module @Wired {
  // CHECK: nsl.wire "src" : !nsl.bits<8>
  %src = nsl.wire "src" : !nsl.bits<8>
  // CHECK: nsl.wire "dst" : !nsl.bits<8>
  %dst = nsl.wire "dst" : !nsl.bits<8>
  // CHECK: nsl.connect %{{.*}}, %{{.*}} : !nsl.bits<8>
  nsl.connect %dst, %src : !nsl.bits<8>
}
