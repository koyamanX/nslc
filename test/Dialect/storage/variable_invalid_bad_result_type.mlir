// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// RUN: nsl-opt --verify-diagnostics %s
//
// FR-013 / FR-019 invariant: `nsl.variable` result type must be
// `!nsl.bits<N>` (per data-model §2.2's hand-written check). A
// `!nsl.mem<...>` result violates this.
// Expects diagnostic substring "result #0" — emitted by TableGen's
// `NSL_AnyBits` result-type constraint (fires before the hand-
// written `VariableOp::verify()` body). Round-5 substring tightened
// during T105 from "result type" to "result #0".

nsl.module @VarHost {
  // expected-error@+1 {{result #0}}
  %bad = "nsl.variable"() {name = "bad"} : () -> !nsl.mem<[16 x !nsl.bits<8>]>
}
