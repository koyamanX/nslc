// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// RUN: nsl-opt --verify-diagnostics %s
//
// FR-013 / FR-019 invariant: `nsl.if` has exactly two regions
// (then, else). Per data-model §2.4 ("two regions") this is enforced
// by TableGen region-count machinery. The generic-form below supplies
// a single empty region, violating the count.
// Added per /speckit-analyze finding F6.
// Expects MLIR-standard region-count diagnostic substring "region"
// once T099 lands.

nsl.module @IfHost {
  %c = nsl.wire "c" : !nsl.bits<1>
  nsl.func @body {
    nsl.seq {
      // expected-error@+1 {{region}}
      "nsl.if"(%c) ({
        ^bb0:
      }) : (!nsl.bits<1>) -> ()
    }
  }
}
