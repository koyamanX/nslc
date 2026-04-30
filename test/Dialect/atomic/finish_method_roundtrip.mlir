// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// RUN: nsl-opt %s | FileCheck %s
// RUN: nsl-opt %s | nsl-opt - | FileCheck %s
//
// FR-010 round-trip for `nsl.finish_method` (design §7 line 917).
// Form: `nsl.finish_method @procInst`. Symbol ref to `nsl.proc`
// per FR-013. Called from a peer module per S21.

// CHECK-LABEL: nsl.module @FinishMethodHost
nsl.module @FinishMethodHost {
  // CHECK: nsl.func @driver
  nsl.func @driver {
    nsl.seq {
      // CHECK: nsl.finish_method @procInst
      nsl.finish_method @procInst
    }
  }
}
