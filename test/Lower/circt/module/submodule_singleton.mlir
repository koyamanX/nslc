// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// test/Lower/circt/module/submodule_singleton.mlir — M6 Phase 4
// (US2) fixture (T036). A singleton `nsl.submodule` (post-M5
// `NSLExplodeSubmodArrayPass`) lowers to `hw.instance` referencing
// the target `hw.module`. Input is `.mlir` (not `.nsl`) because the
// M5 `visit(SubmoduleDecl)` is currently a STUB — the AST → nsl
// lowering for submodule instances has not landed yet, so the only
// way to exercise `nsl.submodule` in a M6 fixture is to feed it
// directly to `nsl-opt -nsl-to-circt`. When the M5 visitor lands
// `SubmoduleDecl` lowering (a future PR), a sibling `.nsl` fixture
// can be added; this fixture continues to pin the IR-level shape.
//
// Note: the Sub module is intentionally port-less (no `nsl.declare`
// pairing) so that the resulting `hw.module @Sub()` takes zero
// operands. Phase 4 doesn't surface per-instance port-connection
// operands on `nsl.submodule` (the M4 dialect carries only
// `templateRef` + `array_size`, no per-port operand list); a future
// M4 amendment + M5 visit(SubmoduleDecl) lowering will close that
// gap, at which point this fixture can grow connection ops.

// RUN: nsl-opt -nsl-to-circt %s | FileCheck %s

nsl.module @Sub {
}
nsl.module @Top {
  nsl.submodule @u : @Sub
}

// CHECK-LABEL: hw.module @Sub
// CHECK-LABEL: hw.module @Top
// CHECK: hw.instance "u" @Sub
// CHECK-NOT: nsl.
