// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// RUN: nsl-opt %s | FileCheck %s
// RUN: nsl-opt %s | nsl-opt - | FileCheck %s
//
// FR-010 round-trip for `nsl.state` (design §7 line 923). `Symbol`,
// per FR-013 parent = `nsl.proc`, one region.

// CHECK-LABEL: nsl.module @StateHost
nsl.module @StateHost {
  // CHECK: nsl.proc @p
  nsl.proc @p {
    nsl.first_state @s0
    // CHECK: nsl.state @s0
    nsl.state @s0 {
      // CHECK: nsl.goto @s1
      nsl.goto @s1
    }
    // CHECK: nsl.state @s1
    nsl.state @s1 {
    }
  }
}
