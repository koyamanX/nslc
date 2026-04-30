// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// RUN: nsl-opt --verify-diagnostics %s
//
// FR-013 / FR-019 invariant: `nsl.struct` requires a `sym_name`
// StringAttr (per data-model §2.1 — `Symbol` trait + hand-written
// presence check). Generic-form so the parser admits the op but the
// verifier rejects the empty/missing sym_name.
// Expects diagnostic substring "sym_name" once T101 lands.

nsl.module @StructHost {
  // expected-error@+1 {{sym_name}}
  "nsl.struct"() ({
    ^bb0:
  }) : () -> ()
}
