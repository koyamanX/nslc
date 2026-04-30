// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// RUN: nsl-opt %s | FileCheck %s
// RUN: nsl-opt %s | nsl-opt - | FileCheck %s
//
// FR-010 round-trip for `nsl.goto` (design §7 line 923; design §10
// lines 1101–1102). Two forms per S25 / FR-013:
//   - **state-form** inside `nsl.state`: target is sibling `nsl.state`
//   - **label-form** inside `nsl.seq`: target is sibling label op
// Per Q2 Option B, the verifier walks parents transitively to find
// `nsl.state` (state form) or `nsl.seq` (label form).

// CHECK-LABEL: nsl.module @GotoHost
nsl.module @GotoHost {
  // CHECK: nsl.proc @p
  nsl.proc @p {
    nsl.first_state @s0
    // State-form goto: parent (transitively) = nsl.state.
    // CHECK: nsl.state @s0
    nsl.state @s0 {
      // CHECK: nsl.goto @s1
      nsl.goto @s1
    }
    // CHECK: nsl.state @s1
    nsl.state @s1 {
      // CHECK: nsl.goto @s0
      nsl.goto @s0
    }
  }
  // Label-form goto: parent (transitively) = nsl.seq.
  // CHECK: nsl.func @stepper
  nsl.func @stepper {
    nsl.seq {
      // CHECK: nsl.goto @label_end
      nsl.goto @label_end
    }
  }
}
