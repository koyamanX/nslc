// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// RUN: nsl-opt --verify-diagnostics %s
//
// FR-013 / FR-019 invariant: `nsl.reg` result type must be in
// {`!nsl.bits<N>`, `!nsl.struct<@T>`} (per data-model §2.2's
// hand-written check). A `!nsl.mem<...>` result violates this.
// We use the generic-form so the parser admits the bad type and
// the verifier rejects it.
// Expects diagnostic substring "result type" once T103 lands.

nsl.module @RegHost {
  // expected-error@+1 {{result type}}
  %bad = "nsl.reg"() {name = "bad", init = 0 : i64} : () -> !nsl.mem<[16 x !nsl.bits<8>]>
}
