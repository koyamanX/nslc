// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// RUN: nsl-opt --verify-diagnostics %s
//
// FR-013 / FR-019 invariant: `nsl.for` checks loop-bound-attrs shape
// (per data-model §2.4 + T108). A non-integer / wrong-shape
// `loop_bound` attribute violates the verifier's expected attr
// schema.
// Expects diagnostic substring "loop" once T108 lands.

nsl.module @ForHost {
  %init = nsl.wire "init" : !nsl.bits<8>
  %cond = nsl.wire "cond" : !nsl.bits<1>
  %step = nsl.wire "step" : !nsl.bits<8>
  nsl.func @loop {
    nsl.seq {
      // expected-error@+1 {{loop}}
      nsl.for %init, %cond, %step : !nsl.bits<8>, !nsl.bits<1>, !nsl.bits<8> attributes {loop_bound = "not-an-int"} {
      }
    }
  }
}
