// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// RUN: nsl-opt --verify-diagnostics %s
//
// FR-013 / FR-019 invariant: `nsl.module` parent must be the builtin
// `mlir::ModuleOp` (per data-model §2.1 — `HasParent<"::mlir::ModuleOp">`).
// A nested `nsl.module @Inner` inside another `nsl.module @Outer`
// violates the trait's immediate-parent expectation.
// Expects verifier diagnostic substring "expects parent op" (standard
// MLIR `HasParent` trait emit text) once T099 lands.

nsl.module @Outer {
  // expected-error@+1 {{expects parent op}}
  nsl.module @Inner {
  }
}
