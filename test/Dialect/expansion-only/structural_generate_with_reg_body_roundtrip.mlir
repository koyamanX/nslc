// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// RUN: nsl-opt %s | FileCheck %s
// RUN: nsl-opt %s | nsl-opt - | FileCheck %s
//
// Post-merge M4-amendment 2026-05-02 (#5): `nsl.reg`'s parent
// constraint is widened from `ParentOneOf<["ModuleOp", "ProcOp"]>`
// to also admit `StructuralGenerateOp`, so a `generate` block may
// declare per-iteration registers in its body. The fixture
// documents the pre-expansion shape; M5's `NSLExpandGeneratePass`
// (slot 2) replicates the body once per iteration and reparents
// the resulting registers to the enclosing `nsl.module`.

// CHECK-LABEL: nsl.module @GenRegHost
nsl.module @GenRegHost {
  // CHECK: nsl.structural_generate
  nsl.structural_generate attributes {lower = 0 : i64, upper = 4 : i64, step = 1 : i64, loop_var = "i"} {
    // CHECK: nsl.reg "buf" : !nsl.bits<8> = 0
    %r = nsl.reg "buf" : !nsl.bits<8> = 0
  }
}
