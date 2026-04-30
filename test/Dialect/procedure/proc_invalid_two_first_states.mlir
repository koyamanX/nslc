// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// RUN: nsl-opt --verify-diagnostics %s
//
// FR-013 / FR-019 invariant: `nsl.proc` may have at most one
// `nsl.first_state` child (per data-model §2.7's hand-written check).
// Two `nsl.first_state` children violate this.
// Expects diagnostic substring "first_state" once T113 lands.

nsl.module @ProcHost {
  // expected-error@+1 {{first_state}}
  nsl.proc @runner {
    nsl.first_state @s0
    nsl.first_state @s1
    nsl.state @s0 {
    }
    nsl.state @s1 {
    }
  }
}
