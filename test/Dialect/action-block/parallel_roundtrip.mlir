// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// RUN: nsl-opt %s | FileCheck %s
// RUN: nsl-opt %s | nsl-opt - | FileCheck %s
//
// FR-010 round-trip for `nsl.parallel` (design §7 line 906). One
// region; concurrency-default action block.

// CHECK-LABEL: nsl.module @ParHost
nsl.module @ParHost {
  // CHECK: nsl.func @body
  nsl.func @body {
    // CHECK: nsl.parallel
    nsl.parallel {
    }
  }
}
