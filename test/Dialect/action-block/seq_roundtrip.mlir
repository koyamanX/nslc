// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// RUN: nsl-opt %s | FileCheck %s
// RUN: nsl-opt %s | nsl-opt - | FileCheck %s
//
// FR-010 round-trip for `nsl.seq` (design §7 line 907). One region;
// per FR-013 parent = `nsl.func` (immediate). Sequential block.

// CHECK-LABEL: nsl.module @SeqHost
nsl.module @SeqHost {
  // CHECK: nsl.func @stepper
  nsl.func @stepper {
    // CHECK: nsl.seq
    nsl.seq {
    }
  }
}
