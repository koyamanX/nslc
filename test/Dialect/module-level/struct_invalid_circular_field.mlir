// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// RUN: nsl-opt --verify-diagnostics %s
//
// FR-013 / FR-019 invariant: `nsl.struct` field-list must be
// non-circular — a struct cannot contain a field whose type is the
// struct itself (directly or transitively). Per data-model §2.1's
// hand-written verifier: `field-list non-circular`.
// Expects diagnostic substring "circular" once T101 lands.

nsl.module @StructHost {
  // expected-error@+1 {{circular}}
  nsl.struct @Cyclic {
    nsl.field_decl "self" : !nsl.struct<@Cyclic>
  }
}
