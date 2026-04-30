// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// RUN: nsl-opt --verify-diagnostics %s
//
// FR-013 / FR-019 invariant: `nsl.structural_generate` loop-bound
// attrs (`lower`, `upper`, `step`) must form a well-shaped expansion
// (per data-model §2.11 + T118 hand-written check). A `step = 0`
// describes an infinite loop and violates the shape rule.
// Expects diagnostic substring "step" once T118 lands.

nsl.module @GenHost {
  // expected-error@+1 {{step}}
  nsl.structural_generate attributes {lower = 0 : i64, upper = 8 : i64, step = 0 : i64} {
  }
}
