// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// RUN: nsl-opt --verify-diagnostics %s
//
// FR-013 / FR-019 invariant: `nsl.mem` result type must be
// `!nsl.mem<[D x T]>` (per data-model §2.2's hand-written check).
// A `!nsl.bits<N>` result violates this.
// Expects diagnostic substring "result type" once T106 lands.

nsl.module @MemHost {
  // expected-error@+1 {{result type}}
  %bad = "nsl.mem"() {name = "bad"} : () -> !nsl.bits<8>
}
