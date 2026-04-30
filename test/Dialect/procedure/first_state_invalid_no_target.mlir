// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// RUN: nsl-opt --verify-diagnostics %s
//
// FR-013 / FR-019 invariant: `nsl.first_state` `target` symref must
// resolve to a sibling `nsl.state` op (per data-model §2.7 + T114
// hand-written check). Pointing at a name that doesn't exist as a
// sibling state violates this.
// Expects diagnostic substring "target" once T114 lands.

nsl.module @FirstStateHost {
  nsl.proc @p {
    // expected-error@+1 {{target}}
    nsl.first_state @nonexistent
    nsl.state @s_other {
    }
  }
}
