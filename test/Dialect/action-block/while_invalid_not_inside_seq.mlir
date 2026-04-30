// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// RUN: nsl-opt --verify-diagnostics %s
//
// FR-013 / FR-019 invariant (Q2 Option B transitive-parent rule):
// `nsl.while` must be enclosed (transitively) by `nsl.seq` (per
// data-model §2.4's hand-written ancestor-walk verifier). Placing it
// directly under `nsl.func` (no enclosing `nsl.seq`) violates the
// rule.
// Expects exact substring "must be enclosed by 'nsl.seq'" once T108
// lands (per data-model §4 + Q2 Option B helper `emitParentMismatch`).

nsl.module @WhileHost {
  %c = nsl.wire "c" : !nsl.bits<1>
  nsl.func @body {
    // expected-error@+1 {{must be enclosed by 'nsl.seq'}}
    nsl.while %c : !nsl.bits<1> {
    }
  }
}
