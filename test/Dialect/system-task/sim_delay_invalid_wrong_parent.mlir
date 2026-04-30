// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// RUN: nsl-opt --verify-diagnostics %s
//
// FR-013 / FR-019 invariant: `nsl.sim_delay` parent ∈ {`nsl.module`,
// `nsl.sim_init`} (per data-model §2.9 — `ParentOneOf<["ModuleOp",
// "SimInitOp"]>`). Placing it inside `nsl.func` violates the trait.
// Expects standard MLIR `ParentOneOf` trait diagnostic substring
// "expects parent op to be one of" once T099 lands.

nsl.module @SimDelayHost {
  nsl.func @body {
    nsl.seq {
      // expected-error@+1 {{expects parent op to be one of}}
      nsl.sim_delay 10
    }
  }
}
