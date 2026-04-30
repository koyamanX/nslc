// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// RUN: nsl-opt %s | FileCheck %s
// RUN: nsl-opt %s | nsl-opt - | FileCheck %s
//
// FR-010 round-trip for `nsl.while` (design §7 line 908). One region;
// per FR-013 + Q2 Option B, **transitive** parent = `nsl.seq` (any-
// ancestor walk, NOT immediate). Cover both immediate-parent and
// nested-via-parallel placements.

// CHECK-LABEL: nsl.module @WhileHost
nsl.module @WhileHost {
  // CHECK: nsl.wire "c" : !nsl.bits<1>
  %c = nsl.wire "c" : !nsl.bits<1>
  // CHECK: nsl.func @loops
  nsl.func @loops {
    nsl.seq {
      // CHECK: nsl.while %{{.*}}
      nsl.while %c {
      }
      // Nested through nsl.parallel — Q2 Option B transitive parent walk.
      nsl.parallel {
        // CHECK: nsl.while %{{.*}}
        nsl.while %c {
        }
      }
    }
  }
}
