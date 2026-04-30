// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// RUN: nsl-opt --verify-diagnostics %s
//
// FR-013 / FR-019 invariant (Q2 Option B transitive-parent rule):
// `nsl.goto` must be enclosed (transitively) by `nsl.seq` (label form)
// OR `nsl.state` (state form) (per data-model §2.8's hand-written
// ancestor-walk verifier). Placing it at the top of `nsl.module`
// (no enclosing seq or state) violates the rule.
// Expects substring "must be enclosed by" once T115 lands. T115
// emits both 'nsl.seq' and 'nsl.state' candidates per Q2 Option B
// helper `emitParentMismatch`.

nsl.module @GotoHost {
  // expected-error@+1 {{must be enclosed by}}
  nsl.goto @somewhere
}
