// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// RUN: nsl-opt %s | FileCheck %s
// RUN: nsl-opt %s | nsl-opt - | FileCheck %s
//
// FR-010 round-trip for `nsl.finish` (design §7 line 916). Per
// FR-013 + Q2 Option B, transitive parent = `nsl.proc` (any-ancestor
// walk). Bare `nsl.finish` form.

// CHECK-LABEL: nsl.module @FinishHost
nsl.module @FinishHost {
  // CHECK: nsl.proc @runner
  nsl.proc @runner {
    nsl.first_state @s0
    // CHECK: nsl.state @s0
    nsl.state @s0 {
      // CHECK: nsl.finish
      nsl.finish
    }
  }
}
