// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// RUN: nsl-opt %s | FileCheck %s
// RUN: nsl-opt %s | nsl-opt - | FileCheck %s
//
// FR-010 round-trip for `nsl.proc` (design §7 lines 921, 958).
// `Symbol`, `SymbolTable`, parent = `nsl.module`,
// `SingleBlockImplicitTerminator<"ProcTerminatorOp">`. Per FR-013,
// at most one `nsl.first_state` child.

// CHECK-LABEL: nsl.module @ProcHost
nsl.module @ProcHost {
  // CHECK: nsl.proc @runner
  nsl.proc @runner {
    // CHECK: nsl.first_state @s0
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
