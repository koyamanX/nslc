// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// RUN: nsl-opt %s | FileCheck %s
// RUN: nsl-opt %s | nsl-opt - | FileCheck %s
//
// FR-010 round-trip for `nsl.state` at module top-level (post-
// amendment #8). Mirrors the NSL grammar shape (`lang.ebnf §6`) where
// `state s { ... }` defs are siblings of `proc p`. The
// `nsl.first_state @<n>` reference in `nsl.proc @p` resolves to the
// module-level `nsl.state @<n>` via FlatSymbolRefAttr.

// CHECK-LABEL: nsl.module @StateAtModule
nsl.module @StateAtModule {
  // CHECK: nsl.proc @p
  nsl.proc @p {
    // CHECK: nsl.first_state @s1
    nsl.first_state @s1
  }
  // CHECK: nsl.state @s1
  nsl.state @s1 {
  }
  // CHECK: nsl.state @s2
  nsl.state @s2 {
  }
}
