// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// RUN: nsl-opt --verify-diagnostics %s
//
// FR-013 / FR-019 invariant: `nsl.state` requires a `sym_name`
// StringAttr (per data-model §2.7 — `Symbol` trait). Generic-form so
// the parser admits the op but the verifier rejects the missing/empty
// `sym_name`.
// Expects diagnostic substring "sym_name" once T099 lands (TableGen
// `Symbol` trait verifier).

nsl.module @StateHost {
  nsl.proc @p {
    nsl.first_state @s0
    // expected-error@+1 {{sym_name}}
    "nsl.state"() ({
      ^bb0:
    }) : () -> ()
  }
}
