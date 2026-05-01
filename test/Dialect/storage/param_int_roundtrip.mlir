// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// RUN: nsl-opt %s | FileCheck %s
// RUN: nsl-opt %s | nsl-opt - | FileCheck %s
//
// Post-merge M4-amendment 2026-05-02 (#4): round-trip for
// `nsl.param_int` (top-level integer parameter per S16; consumed by
// M5's `NSLResolveParamsPass` slot 1 per design §9). Symbol-bearing,
// parent = `mlir::ModuleOp` (top-level placement, sibling of
// `nsl.module`).
// Form: `nsl.param_int @sym_name = value`.

// CHECK: nsl.param_int @N = 8
nsl.param_int @N = 8

// CHECK: nsl.param_int @WIDTH = 16
nsl.param_int @WIDTH = 16

// CHECK: nsl.param_int @DEPTH = 256
nsl.param_int @DEPTH = 256

// CHECK-LABEL: nsl.module @ParamHost
nsl.module @ParamHost {
}
