// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// RUN: nsl-opt --verify-diagnostics %s
//
// FR-013 / FR-019 invariant: `nsl.connect` operand types must be
// strictly equal under `mlir::Type` pointer-equality (per Q3 Option A
// — strict type equality; M5 lowering inserts `nsl.struct_cast` at
// any user-written struct↔bits conversion site, the dialect verifier
// never tolerates implicit reinterpretation).
//
// The connect op's TableGen `assemblyFormat` uses a single
// `type($dst)` token, so the printable surface form forces both
// operands to share a printed type. To express the type mismatch we
// use the generic-form so each operand has its own type field.
// Expects diagnostic substring "operand type" once T102 lands.

nsl.module @Wired {
  %src = nsl.wire "src" : !nsl.bits<8>
  %dst = nsl.wire "dst" : !nsl.bits<16>
  // expected-error@+1 {{operand type}}
  "nsl.connect"(%dst, %src) : (!nsl.bits<16>, !nsl.bits<8>) -> ()
}
