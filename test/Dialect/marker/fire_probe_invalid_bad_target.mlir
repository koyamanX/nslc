// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// RUN: nsl-opt --verify-diagnostics %s
//
// FR-013 / FR-019 invariant: `nsl.fire_probe` `target` symref must
// resolve to a sibling `nsl.func_in` / `nsl.func_out` / `nsl.func_self`
// (per data-model §2.10 + T116 hand-written check). Pointing at a
// name that doesn't exist as a sibling control terminal violates
// this.
// Expects diagnostic substring "target" once T116 lands.

nsl.module @FireProbeHost {
  // expected-error@+1 {{target}}
  nsl.fire_probe @nonexistent_terminal
}
