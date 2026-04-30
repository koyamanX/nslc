// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// RUN: nsl-opt --verify-diagnostics %s
//
// FR-013 / FR-019 invariant: `nsl.module` requires a `sym_name`
// StringAttr (per data-model §2.1 — `Symbol` trait + hand-written
// presence check). The custom-form `nsl.module {}` without `@Name`
// fails to parse, so we exercise the verifier path by constructing
// the op via the generic-form so the parser accepts it but the
// verifier rejects the missing/empty `sym_name`.
// Expects diagnostic substring "sym_name" once T100 lands.

// expected-error@+1 {{sym_name}}
"nsl.module"() ({
  ^bb0:
    "nsl.module_terminator"() : () -> ()
}) : () -> ()
