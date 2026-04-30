// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// RUN: nsl-opt --verify-diagnostics %s
//
// FR-013 / FR-019 invariant: `nsl.goto` `target` symref must resolve
// to a sibling `nsl.state` (state form) or a sibling label op (label
// form) (per data-model §2.8 + T115 hand-written check). Pointing at
// a name that is not declared as either violates this.
// Expects diagnostic substring "target" once T115 lands.

nsl.module @GotoHost {
  nsl.proc @p {
    nsl.first_state @s0
    nsl.state @s0 {
      // expected-error@+1 {{target}}
      nsl.goto @nonexistent_state
    }
  }
}
