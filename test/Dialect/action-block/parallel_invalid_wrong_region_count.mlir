// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// RUN: nsl-opt --verify-diagnostics %s
//
// FR-013 / FR-019 invariant: `nsl.parallel` has exactly one region
// (per data-model §2.4 — "one region"). The generic-form below
// supplies two regions, violating the count.
// Added per /speckit-analyze finding F7.
// Expects MLIR-standard region-count diagnostic substring "region"
// once T099 lands.

nsl.module @ParHost {
  nsl.func @body {
    // expected-error@+1 {{region}}
    "nsl.parallel"() ({
      ^bb0:
    }, {
      ^bb0:
    }) : () -> ()
  }
}
