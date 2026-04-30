// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// RUN: nsl-opt --verify-diagnostics %s
//
// FR-013 / FR-019 invariant: `nsl.func` requires a `sym_name`
// StringAttr (per data-model §2.7 + Q5 Option A' — `Symbol` trait;
// `sym_name` carries the literal dotted form). Generic-form so the
// parser admits the op but the verifier rejects the missing
// `sym_name`.
// Expects diagnostic substring "sym_name" once T099 lands (TableGen
// `Symbol` trait verifier).

nsl.module @FuncHost {
  // expected-error@+1 {{sym_name}}
  "nsl.func"() ({
    ^bb0:
  }) : () -> ()
}
