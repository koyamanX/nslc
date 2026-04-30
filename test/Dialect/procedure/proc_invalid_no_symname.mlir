// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// RUN: nsl-opt --verify-diagnostics %s
//
// FR-013 / FR-019 invariant: `nsl.proc` requires a `sym_name`
// StringAttr (per data-model §2.7 — `Symbol` trait + hand-written
// presence check). Generic-form so the parser admits the op but the
// verifier rejects the missing/empty `sym_name`.
// Expects diagnostic substring "sym_name" once T113 lands.

nsl.module @ProcHost {
  // expected-error@+1 {{sym_name}}
  "nsl.proc"() ({
    ^bb0:
      "nsl.proc_terminator"() : () -> ()
  }) : () -> ()
}
