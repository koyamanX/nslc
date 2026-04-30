// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// RUN: nsl-opt %s | FileCheck %s
// RUN: nsl-opt %s | nsl-opt - | FileCheck %s
//
// FR-010 round-trip for `nsl.first_state` (design §7 line 922). Per
// FR-013 parent = `nsl.proc`; `SymbolRefAttr` resolves to a sibling
// `nsl.state`.

// CHECK-LABEL: nsl.module @FirstStateHost
nsl.module @FirstStateHost {
  // CHECK: nsl.proc @p
  nsl.proc @p {
    // CHECK: nsl.first_state @entry
    nsl.first_state @entry
    // CHECK: nsl.state @entry
    nsl.state @entry {
    }
  }
}
