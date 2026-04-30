// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// RUN: nsl-opt --verify-diagnostics %s
//
// FR-013 / FR-019 invariant (Q2 Option B transitive-parent rule):
// `nsl.for` must be enclosed (transitively) by `nsl.seq` (per
// data-model §2.4's hand-written ancestor-walk verifier). Placing
// it directly under `nsl.func` (no enclosing `nsl.seq`) violates
// the rule.
// Expects exact substring "must be enclosed by 'nsl.seq'" once T108
// lands.

nsl.module @ForHost {
  %init = nsl.wire "init" : !nsl.bits<8>
  %cond = nsl.wire "cond" : !nsl.bits<1>
  %step = nsl.wire "step" : !nsl.bits<8>
  nsl.func @loop {
    // expected-error@+1 {{must be enclosed by 'nsl.seq'}}
    nsl.for %init, %cond, %step : !nsl.bits<8>, !nsl.bits<1>, !nsl.bits<8> {
    }
  }
}
