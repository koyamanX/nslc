// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// RUN: nsl-opt %s | FileCheck %s
// RUN: nsl-opt %s | nsl-opt - | FileCheck %s
//
// FR-010 round-trip for `nsl.sim_finish` (design §7 line 928).
// Sim-only system task. Per FR-013 parent = `nsl.module`.

// CHECK-LABEL: nsl.module @SimFinishHost
nsl.module @SimFinishHost {
  // CHECK: nsl.sim_finish "done"
  nsl.sim_finish "done"
}
