// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// RUN: nsl-opt --verify-diagnostics %s
//
// FR-013 / FR-019 invariant: `nsl.mem` result type must be
// `!nsl.mem<[D x T]>` (per data-model §2.2's hand-written check).
// A `!nsl.bits<N>` result violates this.
// Expects diagnostic substring "result #0" — emitted by TableGen's
// `NSL_AnyMem` result-type constraint (fires before the hand-
// written `MemOp::verify()` body). Round-5 substring tightened
// during T106 from "result type" to "result #0".

nsl.module @MemHost {
  // expected-error@+1 {{result #0}}
  %bad = "nsl.mem"() {name = "bad"} : () -> !nsl.bits<8>
}
