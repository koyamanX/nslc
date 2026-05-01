// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// RUN: nsl-opt -nsl-explode-submod-array %s | FileCheck %s
//
// M5 US4 / FR-016 — `NSLExplodeSubmodArrayPass` (slot 4) for a
// degenerate single-element array. The pass MUST replicate even
// `[1]` to a single `<name>_0` per contract — this preserves the
// invariant that any post-pass IR contains zero array-form
// `nsl.submodule` ops, so downstream consumers (M6) need only
// handle the singleton form.

// CHECK-LABEL: nsl.module @TopArr1
// CHECK-NOT: @inst : @SUB[
nsl.module @TopArr1 {
  // CHECK: nsl.submodule @inst_0 : @SUB
  nsl.submodule @inst : @SUB[1]
}

// CHECK-LABEL: nsl.module @SUB
nsl.module @SUB {
}
