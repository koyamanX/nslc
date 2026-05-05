// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// test/Lower/circt/module/submodule_with_strparam.mlir — M6 Phase 4
// (US2) fixture (T041). A `nsl.param_str` declared at top level (per
// S16 — HDL-binding only) is consumed at submodule instantiation as
// an `hw.instance` parameter wire (string-typed) per design line
// 1256. Input is `.mlir` for the same reasons as
// `submodule_with_param.mlir`.

// RUN: nsl-opt -nsl-to-circt %s | FileCheck %s

nsl.param_str @NAME = "foo"
nsl.module @Sub {
}
nsl.module @Top {
  nsl.submodule @u : @Sub
}

// CHECK-LABEL: hw.module @Top
// CHECK: hw.instance "u" @Sub
// CHECK-SAME: <NAME: none = "foo">
// CHECK-NOT: nsl.
