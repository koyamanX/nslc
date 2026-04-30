// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// RUN: nsl-opt --verify-diagnostics %s
//
// FR-013 / FR-019 invariant: `nsl.transfer` operand types must match
// (per data-model §2.6 — `SameTypeOperands` trait). The TableGen
// `assemblyFormat` prints a single `type($dst)`, so the printable
// form forces both operands to share a printed type. We use the
// generic-form so each operand has its own type field.
// Expects MLIR-standard `SameTypeOperands` trait diagnostic substring
// "same type" once T099 lands.

nsl.module @TransferHost {
  %dst = nsl.wire "dst" : !nsl.bits<8>
  %src = nsl.wire "src" : !nsl.bits<16>
  // expected-error@+1 {{same type}}
  "nsl.transfer"(%dst, %src) : (!nsl.bits<8>, !nsl.bits<16>) -> ()
}
