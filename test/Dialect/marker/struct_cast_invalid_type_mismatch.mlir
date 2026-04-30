// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// RUN: nsl-opt --verify-diagnostics %s
//
// FR-013 / FR-019 invariant: `nsl.struct_cast` source-bits-width
// must equal the target struct's totalWidth (per data-model §2.10 +
// T117 hand-written check). Casting a `!nsl.bits<8>` to a 16-bit
// struct violates this.
// Expects diagnostic substring "width" once T117 lands.

nsl.module @StructCastHost {
  nsl.struct @Wide {
    nsl.field_decl "lo" : !nsl.bits<8>
    nsl.field_decl "hi" : !nsl.bits<8>
  }
  %narrow = nsl.wire "narrow" : !nsl.bits<8>
  // expected-error@+1 {{width}}
  %bad = nsl.struct_cast %narrow : !nsl.bits<8> to !nsl.struct<@Wide>
}
