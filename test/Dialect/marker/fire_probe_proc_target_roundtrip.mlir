// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// RUN: nsl-opt %s | FileCheck %s
// RUN: nsl-opt %s | nsl-opt - | FileCheck %s
//
// Post-merge M4-amendment 2026-05-02 (#5): `nsl.fire_probe`
// accepts `nsl.proc` (proc_name, Symbol) as a sibling target in
// addition to the original `func_in`/`func_out`/`func_self` set.
// Per S27 (constructive), control-terminal AND proc_name
// identifiers may be referenced as 1-bit values.

// CHECK-LABEL: nsl.module @ProcTargetHost
nsl.module @ProcTargetHost {
  // CHECK: nsl.proc @start
  nsl.proc @start {
  }
  // CHECK: nsl.fire_probe @start
  nsl.fire_probe @start
}
