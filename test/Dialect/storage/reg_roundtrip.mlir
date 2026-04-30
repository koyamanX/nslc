// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// RUN: nsl-opt %s | FileCheck %s
// RUN: nsl-opt %s | nsl-opt - | FileCheck %s
//
// FR-010 round-trip for `nsl.reg` (design §7 line 892). Form:
// `nsl.reg "name" : !nsl.bits<N> = init`. Carries init attribute;
// per FR-013 result type ∈ {`!nsl.bits<N>`, `!nsl.struct<@T>`}.

// CHECK-LABEL: nsl.module @RegHost
nsl.module @RegHost {
  // CHECK: nsl.reg "q" : !nsl.bits<8> = 0
  nsl.reg "q" : !nsl.bits<8> = 0
  // CHECK: nsl.reg "ctr" : !nsl.bits<4> = 0
  nsl.reg "ctr" : !nsl.bits<4> = 0
  // CHECK: nsl.reg "cfg" : !nsl.bits<16> = 42
  nsl.reg "cfg" : !nsl.bits<16> = 42
  // CHECK: nsl.struct @S
  nsl.struct @S {
    nsl.field "x" : !nsl.bits<8>
  }
  // CHECK: nsl.reg "rec" : !nsl.struct<@S>
  nsl.reg "rec" : !nsl.struct<@S>
}
