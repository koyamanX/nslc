// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// RUN: nsl-opt --verify-diagnostics %s
//
// FR-013 / FR-019 invariant: `nsl.wire` result type must be
// `!nsl.bits<N>` (per data-model §2.2's hand-written check — wires
// never carry struct or mem). A `!nsl.struct<@T>` result violates
// this.
// Expects diagnostic substring "result #0" — emitted by TableGen's
// `NSL_AnyBits` result-type constraint (fires before the hand-
// written `WireOp::verify()` body). Round-5 substring tightened
// during T104 from "result type" to "result #0".

nsl.module @WireHost {
  nsl.struct @S {
    nsl.field_decl "x" : !nsl.bits<8>
  }
  // expected-error@+1 {{result #0}}
  %bad = "nsl.wire"() {name = "bad"} : () -> !nsl.struct<@S>
}
