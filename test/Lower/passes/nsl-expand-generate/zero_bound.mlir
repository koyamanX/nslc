// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// RUN: nsl-opt --mlir-very-unsafe-disable-verifier-on-parsing -nsl-expand-generate %s | FileCheck %s
//
// M5 US2 / FR-014 — edge case: zero-iteration generate
// (`upper == lower`). Pass MUST erase the op AND emit zero body
// copies (the body is unreachable). Post-pass module body is
// empty.

// CHECK-LABEL: nsl.module @GenZero
// CHECK-NOT: nsl.structural_generate
// CHECK-NOT: nsl.reg
nsl.module @GenZero {
  nsl.structural_generate attributes {lower = 0 : i64, upper = 0 : i64, step = 1 : i64, loop_var = "i"} {
    nsl.reg "buf_%i%" : !nsl.bits<8>
  }
}
