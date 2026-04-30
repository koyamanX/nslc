// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// RUN: nsl-opt --verify-diagnostics %s
//
// FR-013 / FR-019 invariant: `nsl.field` `index` integer attr must
// fall in [0, struct.numFields) (per data-model §2.10 + T117 hand-
// written check). An out-of-range index violates this.
// Expects diagnostic substring "index" once T117 lands.

nsl.module @FieldHost {
  nsl.struct @Pair {
    nsl.field_decl "lo" : !nsl.bits<8>
    nsl.field_decl "hi" : !nsl.bits<8>
  }
  %r = nsl.reg "r" : !nsl.struct<@Pair>
  // expected-error@+1 {{index}}
  %bad = nsl.field %r {index = 99 : i64} : !nsl.struct<@Pair> -> !nsl.bits<8>
}
