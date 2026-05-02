// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// RUN: nsl-opt -nsl-inline-internal-func %s | FileCheck %s
//
// M5 T105 / FR-017 — `NSLInlineInternalFuncPass` (slot 5 of the
// 6-slot pipeline per `pass-pipeline.contract.md` §2). At M5 the
// pass STAYS a registered no-op slot per Clarifications Q3 → Option
// B: it reserves the pipeline ABI (pass-name + signature + position)
// so a future PR can fill in functional `func_self` inlining without
// amending the M5 spec.
//
// Cited design: `specs/008-m5-structural-passes/spec.md` FR-017;
// `pass-pipeline.contract.md` §2 row 5; Clarifications Q3 → Option B.
//
// Per FR-017 the contract for this slot at M5 is: IR is
// byte-identical to input. This fixture asserts that round-trip
// invariant on a trivial input. When functional inlining lands
// post-M5, this fixture is replaced by behavioural FileCheck cases
// (the no-op invariant becomes invalid — that change is a contract
// amendment per Principle VII).

// CHECK-LABEL: nsl.module @M
nsl.module @M {
  // CHECK: nsl.func @f
  nsl.func @f { }
}
