// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// RUN: nsl-opt %s | FileCheck %s
// RUN: nsl-opt %s | nsl-opt - | FileCheck %s
//
// Post-merge M4-amendment 2026-05-02 (#4): round-trip for
// `nsl.param_str` (top-level string parameter per S16; consumed by
// M5's `NSLResolveParamsPass` slot 1 per design §9). Symbol-bearing,
// parent = `mlir::ModuleOp` (top-level placement, sibling of
// `nsl.module`).
// Form: `nsl.param_str @sym_name = "value"`.

// CHECK: nsl.param_str @WIDTH = "8"
nsl.param_str @WIDTH = "8"

// CHECK: nsl.param_str @MODE = "async"
nsl.param_str @MODE = "async"

// CHECK-LABEL: nsl.module @StrHost
nsl.module @StrHost {
}
