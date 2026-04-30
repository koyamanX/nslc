// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// RUN: nsl-opt --verify-diagnostics %s
//
// FR-013 / FR-019 invariant: `nsl.alt` requires ≥ 1 child whose kind
// is `nsl.case` or `nsl.default` (per data-model §2.4's hand-written
// check). An empty body violates this.
// Expects diagnostic substring "case-or-default" once T107 lands.

nsl.module @AltHost {
  nsl.func @body {
    nsl.seq {
      // expected-error@+1 {{case-or-default}}
      nsl.alt {
      }
    }
  }
}
