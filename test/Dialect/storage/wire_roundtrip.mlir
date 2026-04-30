// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// RUN: nsl-opt %s | FileCheck %s
// RUN: nsl-opt %s | nsl-opt - | FileCheck %s
//
// FR-010 round-trip for `nsl.wire` (design §7 line 893). Result
// type per FR-013 is `!nsl.bits<N>` (wires never carry struct).

// CHECK-LABEL: nsl.module @WireHost
nsl.module @WireHost {
  // CHECK: nsl.wire "a" : !nsl.bits<1>
  nsl.wire "a" : !nsl.bits<1>
  // CHECK: nsl.wire "bus" : !nsl.bits<32>
  nsl.wire "bus" : !nsl.bits<32>
}
