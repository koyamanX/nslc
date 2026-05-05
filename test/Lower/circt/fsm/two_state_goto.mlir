// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// test/Lower/circt/fsm/two_state_goto.mlir — M6 Phase 5 (US3)
// fixture (T046). Two-state FSM with state-form `goto` (S25). The
// M5 visitor's `GotoStmt` lowering is currently a stub (see
// `lib/Lower/ASTToMLIR.cpp` STUB list around line 2105) — so this
// fixture authors the `nsl.goto` op directly in `.mlir` form and
// drives `nsl-opt -nsl-to-circt` rather than `nslc -emit=hw`.
//
// Lowering shape (per `circt-lowering.contract.md` §1):
//   nsl.proc @P {
//     nsl.first_state @s0
//     nsl.state @s0 { nsl.goto @s1 }
//     nsl.state @s1 { }
//   }
//   →
//   fsm.machine @P {
//     fsm.state @s0 transitions { fsm.transition @s1 }
//     fsm.state @s1
//   }

// RUN: nsl-opt -nsl-to-circt %s | FileCheck %s

nsl.declare @M {
}
nsl.module @M {
  nsl.proc @p {
    nsl.first_state @s0
    nsl.state @s0 {
      nsl.goto @s1
    }
    nsl.state @s1 {
    }
  }
}

// CHECK-LABEL: fsm.machine @p
// CHECK-SAME: initialState = "s0"
// CHECK: fsm.state @s0
// CHECK: fsm.transition @s1
// CHECK: fsm.state @s1
// CHECK-NOT: nsl.proc
// CHECK-NOT: nsl.state
// CHECK-NOT: nsl.goto
// CHECK-NOT: nsl.first_state
