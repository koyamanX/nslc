// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// RUN: nsl-opt --verify-diagnostics %s
//
// FR-013 / FR-019 invariant (Q2 Option B transitive-parent rule):
// `nsl.finish` must be enclosed (transitively) by `nsl.proc` (per
// data-model §2.6's hand-written ancestor-walk verifier). Placing
// it inside `nsl.func` (no enclosing `nsl.proc`) violates the rule.
// Expects exact substring "must be enclosed by 'nsl.proc'" once T112
// lands (per data-model §4 + Q2 Option B helper `emitParentMismatch`).

nsl.module @FinishHost {
  nsl.func @body {
    nsl.seq {
      // expected-error@+1 {{must be enclosed by 'nsl.proc'}}
      nsl.finish
    }
  }
}
