// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// RUN: nsl-opt %s | FileCheck %s
// RUN: nsl-opt %s | nsl-opt - | FileCheck %s
//
// FR-010 round-trip for `nsl.submodule` (design §7 line 888). Symbol
// trait + parent = `nsl.module`. Form: `nsl.submodule @Inst : @Template`.

// CHECK-LABEL: nsl.module @Top
nsl.module @Top {
  // CHECK: nsl.submodule @u_inner : @Inner
  nsl.submodule @u_inner : @Inner
  // CHECK: nsl.submodule @u_other : @Other
  nsl.submodule @u_other : @Other
}

// CHECK-LABEL: nsl.module @Inner
nsl.module @Inner {
}

// CHECK-LABEL: nsl.module @Other
nsl.module @Other {
}
