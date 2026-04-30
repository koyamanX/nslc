// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// RUN: nsl-opt %s | FileCheck %s
// RUN: nsl-opt %s | nsl-opt - | FileCheck %s
//
// FR-010 round-trip for `nsl.sim_delay` (design §7 line 929). Sim-
// only; integer-literal cycles attribute. Often nested under
// `nsl.sim_init` but tested standalone here for the per-op gate.

// CHECK-LABEL: nsl.module @SimDelayHost
nsl.module @SimDelayHost {
  // CHECK: nsl.sim_init
  nsl.sim_init {
    // CHECK: nsl.sim_delay 1
    nsl.sim_delay 1
    // CHECK: nsl.sim_delay 100
    nsl.sim_delay 100
    // CHECK: nsl.sim_delay 9999
    nsl.sim_delay 9999
  }
}
