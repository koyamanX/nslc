// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// test/Lower/circt/fsm/seq_inside_func.mlir — M6 Phase 5 (US3)
// fixture (T049). `nsl::SeqOp` inside a `nsl::FuncOp` lowers to a
// `fsm::MachineOp` with auto-generated `seq_N` state names per
// design §10 line 1219. Each label-form `nsl.goto` inside the seq
// becomes a `fsm.transition` to the matching `seq_N`.
//
// The M5 visitor's `LabeledStmt` and `GotoStmt` lowerings are
// stubs — so this fixture authors the `nsl.seq` / `nsl.goto`
// shape directly in `.mlir` form via `nsl-opt -nsl-to-circt`.
//
// Lowering shape:
//   nsl.func @F {
//     nsl.seq {
//       nsl.goto @label1
//     }
//   }
//   →
//   fsm.machine @F attributes { initialState = "seq_0" } {
//     fsm.state @seq_0 transitions { fsm.transition @seq_1 }
//     fsm.state @seq_1
//   }
//
// The seq-form `nsl.goto` resolves to its sibling `seq_N` by
// counting label position (Phase 5 minimal — no labelled-stmt
// support since M5's visitor doesn't emit them). The goto here
// targets the implicit "next state" by symbol name `@label1`,
// which the FSM pattern translates by checking the seq's symbol
// table for the same name.
//
// Phase 5 simplification: the seq pattern produces ONE state per
// goto plus an entry state (seq_0). Subsequent leaf-op patterns
// in Phase 6 will populate state bodies with arithmetic /
// transfer / etc. ops; at Phase 5 the bodies are empty.

// RUN: nsl-opt -nsl-to-circt %s | FileCheck %s

nsl.module @M {
  nsl.func @F {
    nsl.seq {
      nsl.goto @label1
    }
  }
}

// CHECK-LABEL: fsm.machine @F
// CHECK-SAME: initialState = "seq_0"
// CHECK: fsm.state @seq_0
// CHECK: fsm.transition @seq_1
// CHECK: fsm.state @seq_1
// CHECK-NOT: nsl.func
// CHECK-NOT: nsl.seq
// CHECK-NOT: nsl.goto
