// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// RUN: nsl-opt --mlir-very-unsafe-disable-verifier-on-parsing -nsl-expand-generate %s | FileCheck %s
//
// M5 US2 / FR-014 — acceptance scenario 3 (`spec.md:230`): nested
// generates whose inner bound is a constant referring to an outer
// loop var would yield the triangular number. At M5's frozen
// dialect surface the inner upper bound is an I64Attr (literal), so
// dynamic dependency on outer-i isn't expressible directly — but
// straight nesting of two generates with literal bounds DOES work
// and asserts the recursive-walk shape of the pass.
//
// This fixture exercises a 2x3 = 6-replica nested generate. The
// outer pass over the IR walks the outer generate and clones its
// body twice; each clone's body still contains a nested generate,
// which the same pass invocation must handle in its post-clone
// walk (or via re-walking until fixed-point). FR-014 doesn't pin
// the strategy — both are valid as long as the post-pass IR has
// zero nsl.structural_generate ops.

// CHECK-LABEL: nsl.module @GenNested
// CHECK-NOT: nsl.structural_generate
nsl.module @GenNested {
  // Outer 0..2, inner 0..3 — total 6 replicas of the inner body.
  // CHECK: nsl.reg "cell_0_0" : !nsl.bits<8>
  // CHECK: nsl.reg "cell_0_1" : !nsl.bits<8>
  // CHECK: nsl.reg "cell_0_2" : !nsl.bits<8>
  // CHECK: nsl.reg "cell_1_0" : !nsl.bits<8>
  // CHECK: nsl.reg "cell_1_1" : !nsl.bits<8>
  // CHECK: nsl.reg "cell_1_2" : !nsl.bits<8>
  nsl.structural_generate attributes {lower = 0 : i64, upper = 2 : i64, step = 1 : i64, loop_var = "i"} {
    nsl.structural_generate attributes {lower = 0 : i64, upper = 3 : i64, step = 1 : i64, loop_var = "j"} {
      nsl.reg "cell_%i%_%j%" : !nsl.bits<8>
    }
  }
}
