// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// RUN: nsl-opt %s | FileCheck %s
// RUN: nsl-opt %s | nsl-opt - | FileCheck %s
//
// FR-010 round-trip for `nsl.sim_display` (design §7 line 927).
// Sim-only; format-string + var-args. Per FR-013 parent =
// `nsl.module`.

// CHECK-LABEL: nsl.module @SimDisplayHost
nsl.module @SimDisplayHost {
  // CHECK: nsl.wire "x" : !nsl.bits<8>
  %x = nsl.wire "x" : !nsl.bits<8>
  // CHECK: nsl.sim_display "x = %d", %{{.*}}
  nsl.sim_display "x = %d", %x
  // CHECK: nsl.sim_display "tick"
  nsl.sim_display "tick"
}
