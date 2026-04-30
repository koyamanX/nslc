// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// RUN: nsl-opt --verify-diagnostics %s
//
// FR-013 / FR-019 invariant: `nsl.clocked_transfer` operand types
// must match (per data-model §2.6 — `SameTypeOperands` trait).
// We use the generic-form to express the type mismatch.
// Expects MLIR-standard `SameTypeOperands` trait diagnostic substring
// "same type" once T099 lands.

nsl.module @ClockedHost {
  %q = nsl.reg "q" : !nsl.bits<8> = 0
  %src = nsl.wire "src" : !nsl.bits<16>
  // expected-error@+1 {{same type}}
  "nsl.clocked_transfer"(%q, %src) : (!nsl.bits<8>, !nsl.bits<16>) -> ()
}
