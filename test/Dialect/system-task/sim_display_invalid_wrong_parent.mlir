// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// RUN: nsl-opt --verify-diagnostics %s
//
// FR-013 / FR-019 invariant: `nsl.sim_display` parent ∈ {`nsl.module`,
// `nsl.sim_init`} (per data-model §2.9 — `ParentOneOf<["ModuleOp",
// "SimInitOp"]>`). Placing it directly under the builtin
// `mlir::ModuleOp` violates the trait.
// Expects standard MLIR `ParentOneOf` trait diagnostic substring
// "expects parent op to be one of" once T099 lands.

// expected-error@+1 {{expects parent op to be one of}}
nsl.sim_display "tick"
