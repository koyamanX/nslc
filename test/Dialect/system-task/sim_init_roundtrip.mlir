// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// RUN: nsl-opt %s | FileCheck %s
// RUN: nsl-opt %s | nsl-opt - | FileCheck %s
//
// FR-010 round-trip for `nsl.sim_init` (design §7 line 929). Sim-
// only; one-region body holding sim_delay / sim_display children.
// Per S29, _init block placement is module-level.

// CHECK-LABEL: nsl.module @SimInitHost
nsl.module @SimInitHost {
  // CHECK: nsl.sim_init
  nsl.sim_init {
    // CHECK: nsl.sim_delay 10
    nsl.sim_delay 10
    // CHECK: nsl.sim_display "init done"
    nsl.sim_display "init done"
  }
}
