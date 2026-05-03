// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// RUN: nsl-opt --verify-diagnostics %s
//
// FR-013 / FR-019 invariant: `nsl.field_decl` parent must be
// `nsl.struct` (per data-model §2.10 — `HasParent<"StructOp">`).
// Placing it directly inside `nsl.module` violates the trait.
// Expects the standard MLIR `HasParent` diagnostic substring
// "expects parent op 'nsl.struct'".

nsl.module @FieldDeclHost {
  // expected-error@+1 {{expects parent op 'nsl.struct'}}
  nsl.field_decl "stray" : !nsl.bits<8>
}
