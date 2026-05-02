// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// RUN: nsl-opt --mlir-very-unsafe-disable-verifier-on-parsing -nsl-expand-generate %s | FileCheck %s
//
// M5 US2 / FR-014 — edge case: single-iteration generate
// (`upper == lower + step`). Pass produces exactly one body copy
// (loop_var resolved to `lower`) and erases the op.

// CHECK-LABEL: nsl.module @GenOne
// CHECK-NOT: nsl.structural_generate
nsl.module @GenOne {
  // CHECK: nsl.reg "buf_0" : !nsl.bits<8>
  nsl.structural_generate attributes {lower = 0 : i64, upper = 1 : i64, step = 1 : i64, loop_var = "i"} {
    nsl.reg "buf_%i%" : !nsl.bits<8>
  }
}
