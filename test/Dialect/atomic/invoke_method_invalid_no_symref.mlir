// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// RUN: nsl-opt --verify-diagnostics %s
//
// FR-013 / FR-019 invariant: `nsl.invoke_method` requires a
// `callee` `FlatSymbolRefAttr` (per data-model §2.6). The
// generic-form here omits the attribute entirely; MLIR's
// auto-generated TableGen-trait-only verifier rejects it.
// Expects substring "callee" once T099 lands.

nsl.module @InvokeMethodHost {
  nsl.func @driver {
    nsl.seq {
      // expected-error@+1 {{callee}}
      "nsl.invoke_method"() : () -> ()
    }
  }
}
