// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// RUN: nsl-opt %s | FileCheck %s
// RUN: nsl-opt %s | nsl-opt - | FileCheck %s
//
// FR-010 round-trip for `nsl.struct` (module-level structural type
// declaration; design §7 line 887). Multiple `nsl.field` children
// with mixed bit-widths.

// CHECK-LABEL: nsl.module @StructHost
nsl.module @StructHost {
  // CHECK: nsl.struct @Pair
  nsl.struct @Pair {
    // CHECK: nsl.field "lo" : !nsl.bits<4>
    nsl.field "lo" : !nsl.bits<4>
    // CHECK: nsl.field "hi" : !nsl.bits<4>
    nsl.field "hi" : !nsl.bits<4>
  }
  // CHECK: nsl.struct @Wide
  nsl.struct @Wide {
    // CHECK: nsl.field "a" : !nsl.bits<32>
    nsl.field "a" : !nsl.bits<32>
    // CHECK: nsl.field "b" : !nsl.bits<32>
    nsl.field "b" : !nsl.bits<32>
    // CHECK: nsl.field "c" : !nsl.bits<32>
    nsl.field "c" : !nsl.bits<32>
  }
}
