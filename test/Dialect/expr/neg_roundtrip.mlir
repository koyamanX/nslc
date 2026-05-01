// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// RUN: nsl-opt %s | FileCheck %s
// RUN: nsl-opt %s | nsl-opt - | FileCheck %s
//
// Post-merge M4-amendment 2026-05-02 (Phase A cluster 5): round-trip
// for `nsl.neg` (Pure + SameOperandsAndResultType; NSL unary `-` per
// EBNF §11 line 630). Lowers to `comb.sub 0, %a` at M6.

// CHECK-LABEL: nsl.module @NegHost
nsl.module @NegHost {
  // CHECK: %{{.*}} = nsl.constant 5 : !nsl.bits<8>
  %a = nsl.constant 5 : !nsl.bits<8>
  // CHECK: %{{.*}} = nsl.neg %{{.*}} : !nsl.bits<8>
  %r = nsl.neg %a : !nsl.bits<8>
  // CHECK: nsl.wire "dst" : !nsl.bits<8>
  %dst = nsl.wire "dst" : !nsl.bits<8>
  // CHECK: nsl.transfer %{{.*}}, %{{.*}} : !nsl.bits<8>
  nsl.transfer %dst, %r : !nsl.bits<8>
}
