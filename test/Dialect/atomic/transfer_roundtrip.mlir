// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// RUN: nsl-opt %s | FileCheck %s
// RUN: nsl-opt %s | nsl-opt - | FileCheck %s
//
// FR-010 round-trip for `nsl.transfer` (design §7 line 912; wire-
// style `=`). Carries `SameOperandsElementType` + `SameOperandsShape`
// traits. Form: `nsl.transfer %dst, %src : !nsl.bits<N>`.

// CHECK-LABEL: nsl.module @TransferHost
nsl.module @TransferHost {
  // CHECK: nsl.wire "dst" : !nsl.bits<8>
  %dst = nsl.wire "dst" : !nsl.bits<8>
  // CHECK: nsl.wire "src" : !nsl.bits<8>
  %src = nsl.wire "src" : !nsl.bits<8>
  // CHECK: nsl.transfer %{{.*}}, %{{.*}} : !nsl.bits<8>
  nsl.transfer %dst, %src : !nsl.bits<8>
}
