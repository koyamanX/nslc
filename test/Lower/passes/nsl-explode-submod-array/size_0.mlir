// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// RUN: nsl-opt -nsl-explode-submod-array %s | FileCheck %s
//
// M5 US4 / FR-016 — degenerate zero-element array. The op
// represents zero submodule instances — it is structurally
// "vacuous". Per the explode pass: the array-form op is erased
// outright (no replicas emitted). The post-pass IR contains zero
// `nsl.submodule` ops referencing @inst.
//
// (Whether `array_size = 0` is a Sema-rejected source-language
// pattern is an S-constraint question deferred to M3+; at the
// dialect level the verifier permits any non-negative I64. The
// explode pass treats zero as "erase, no replicas".)

// CHECK-LABEL: nsl.module @TopArr0
// CHECK-NOT: nsl.submodule
nsl.module @TopArr0 {
  nsl.submodule @inst : @SUB[0]
}

// CHECK-LABEL: nsl.module @SUB
nsl.module @SUB {
}
