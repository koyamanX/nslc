// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// RUN: nsl-opt %s | FileCheck %s
// RUN: nsl-opt %s | nsl-opt - | FileCheck %s
//
// Post-merge M4-amendment 2026-05-02 (#4): round-trip for
// `nsl.submodule` array form. The OPTIONAL `array_size` `I64Attr`
// describes the array size of the submodule instance (NSL
// `SUB[3] inst;`); when present the printer emits
// `@inst : @SUB[3]`. M5's `NSLExplodeSubmodArrayPass` (slot 4 per
// design §9) consumes the array form per FR-016 by replicating each
// entry as a sibling singleton submodule. The existing
// `submodule_roundtrip.mlir` covers the singleton (backward-compat)
// form; this fixture covers the array form.

// CHECK-LABEL: nsl.module @TopArr
nsl.module @TopArr {
  // CHECK: nsl.submodule @u_arr3 : @Inner[3]
  nsl.submodule @u_arr3 : @Inner[3]
  // CHECK: nsl.submodule @u_arr8 : @Other[8]
  nsl.submodule @u_arr8 : @Other[8]
  // CHECK: nsl.submodule @u_singleton : @Inner
  nsl.submodule @u_singleton : @Inner
}

// CHECK-LABEL: nsl.module @Inner
nsl.module @Inner {
}

// CHECK-LABEL: nsl.module @Other
nsl.module @Other {
}
