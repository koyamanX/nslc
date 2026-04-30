// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// RUN: nsl-opt --verify-diagnostics %s
//
// FR-013 / FR-019 invariant: `nsl.field` result type must match the
// declared field's type at the chosen index (per data-model §2.10 +
// T117 hand-written check). Picking index 0 (`"lo" : !nsl.bits<8>`)
// but declaring the result as `!nsl.bits<16>` violates this.
// Expects diagnostic substring "result type" once T117 lands.

nsl.module @FieldHost {
  nsl.struct @Pair {
    nsl.field_decl "lo" : !nsl.bits<8>
    nsl.field_decl "hi" : !nsl.bits<8>
  }
  %r = nsl.reg "r" : !nsl.struct<@Pair>
  // expected-error@+1 {{result type}}
  %bad = nsl.field %r {index = 0 : i64} : !nsl.struct<@Pair> -> !nsl.bits<16>
}
