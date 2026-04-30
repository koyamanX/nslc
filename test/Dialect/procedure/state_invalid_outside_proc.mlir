// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// RUN: nsl-opt --verify-diagnostics %s
//
// FR-013 / FR-019 invariant: `nsl.state` parent must be `nsl.proc`
// (per data-model §2.7 — `HasParent<"ProcOp">`). Placing it directly
// under `nsl.module` violates the trait.
// Expects standard MLIR trait diagnostic substring "expects parent op"
// once T099 lands.

nsl.module @StateHost {
  // expected-error@+1 {{expects parent op}}
  nsl.state @s0 {
  }
}
