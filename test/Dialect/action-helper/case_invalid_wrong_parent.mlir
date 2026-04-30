// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// RUN: nsl-opt --verify-diagnostics %s
//
// FR-013 / FR-019 invariant: `nsl.case` parent ∈ {`nsl.alt`,
// `nsl.any`} (variadic `ParentOneOf` per data-model §2.5). Placing
// it directly under `nsl.seq` (a non-alt/any parent) violates the
// trait.
// Expects standard MLIR `ParentOneOf` trait diagnostic substring
// "expects parent op to be one of" once T099 lands.

nsl.module @CaseHost {
  %c = nsl.wire "c" : !nsl.bits<1>
  nsl.func @body {
    nsl.seq {
      // expected-error@+1 {{expects parent op to be one of}}
      nsl.case %c : !nsl.bits<1> {
      }
    }
  }
}
