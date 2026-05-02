// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// RUN: nsl-opt %s | FileCheck %s
// RUN: nsl-opt %s | nsl-opt - | FileCheck %s
//
// Post-merge M4-amendment 2026-05-02 (#5): `nsl.variable`'s
// result-type constraint is widened from `NSL_AnyBits` to
// `NSL_BitsOrStruct`, enabling the FR-015 per-field SSA-split
// chain in `NSLExpandVariablesPass`. The original bit-typed form
// is exercised by `variable_roundtrip.mlir`; this fixture covers
// the struct-typed form.

// CHECK-LABEL: nsl.struct @T
nsl.struct @T {
  // CHECK: nsl.field_decl "a" : !nsl.bits<8>
  nsl.field_decl "a" : !nsl.bits<8>
  // CHECK: nsl.field_decl "b" : !nsl.bits<8>
  nsl.field_decl "b" : !nsl.bits<8>
}

// CHECK-LABEL: nsl.module @VarStructHost
nsl.module @VarStructHost {
  // CHECK: %{{.*}} = nsl.variable "s" : !nsl.struct<@T>
  %s = nsl.variable "s" : !nsl.struct<@T>
}
