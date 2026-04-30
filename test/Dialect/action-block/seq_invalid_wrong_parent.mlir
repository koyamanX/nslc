// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// RUN: nsl-opt --verify-diagnostics %s
//
// FR-013 / FR-019 invariant: `nsl.seq` parent must be `nsl.func`
// (per data-model §2.4 — `HasParent<"NSL_FuncOp">`, immediate-parent).
// Placing it directly under `nsl.module` violates the trait.
// Expects standard MLIR trait diagnostic substring "expects parent op"
// once T099 lands.

nsl.module @SeqHost {
  // expected-error@+1 {{expects parent op}}
  nsl.seq {
  }
}
