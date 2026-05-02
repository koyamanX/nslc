// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// RUN: nsl-opt %s | FileCheck %s
// RUN: nsl-opt %s | nsl-opt - | FileCheck %s
//
// Post-merge M4-amendment 2026-05-02 (#5): `nsl.fire_probe` placed
// inside an `nsl.proc` body accepts a sibling `nsl.state`
// (state_name, Symbol) as its target. This complements the
// module-scope sibling lookup that handles
// `nsl.func_in`/`nsl.func_out`/`nsl.func_self`/`nsl.proc` cases.

// CHECK-LABEL: nsl.module @StateTargetHost
nsl.module @StateTargetHost {
  // CHECK: nsl.proc @driver
  nsl.proc @driver {
    // CHECK: nsl.state @running
    nsl.state @running {
    }
    // CHECK: nsl.fire_probe @running
    nsl.fire_probe @running
  }
}
