// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// RUN: nsl-opt %s | FileCheck %s
// RUN: nsl-opt %s | nsl-opt - | FileCheck %s
//
// FR-018 type round-trip: `!nsl.struct<@T>` referring to a sibling
// `nsl.struct @T { ... }`. Two-pass round-trip per FR-017.

// CHECK-LABEL: nsl.module @struct_typed
nsl.module @struct_typed {
  // CHECK: nsl.struct @MyRec
  nsl.struct @MyRec {
    nsl.field "lo" : !nsl.bits<8>
    nsl.field "hi" : !nsl.bits<8>
  }
  // CHECK: nsl.reg "r" : !nsl.struct<@MyRec>
  nsl.reg "r" : !nsl.struct<@MyRec>
}
