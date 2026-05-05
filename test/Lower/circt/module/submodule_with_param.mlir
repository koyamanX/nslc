// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// test/Lower/circt/module/submodule_with_param.mlir — M6 Phase 4
// (US2) fixture (T037). A `nsl.param_int` declared at top level
// (per S16 — HDL-binding only) is consumed at submodule instantiation
// as an `hw.instance` parameter wire (i32-typed) per design line
// 1255. Input is `.mlir` because (a) S16 forbids `param_int` in
// pure-NSL source so it cannot come from `nslc`, and (b) M5
// `visit(SubmoduleDecl)` is stubbed.
//
// Same port-less-Sub note as `submodule_singleton.mlir`: Phase 4
// doesn't surface per-instance port operands.

// RUN: nsl-opt -nsl-to-circt %s | FileCheck %s

nsl.param_int @N = 8
nsl.module @Sub {
}
nsl.module @Top {
  nsl.submodule @u : @Sub
}

// CHECK-LABEL: hw.module @Top
// CHECK: hw.instance "u" @Sub
// CHECK-SAME: <N: i32 = 8>
// CHECK-NOT: nsl.
