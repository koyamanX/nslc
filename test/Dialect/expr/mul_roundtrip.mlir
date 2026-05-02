// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// RUN: nsl-opt %s | FileCheck %s
// RUN: nsl-opt %s | nsl-opt - | FileCheck %s
//
// Post-merge M4-amendment 2026-05-02 (Phase A cluster 1+3+4): round-trip
// for `nsl.mul` (Pure + SameOperandsAndResultType + Commutative; NSL `*`
// per EBNF §11 line 624). Lowers to `comb.mul` at M6 per design §10.

// CHECK-LABEL: nsl.module @MulHost
nsl.module @MulHost {
  // CHECK: %{{.*}} = nsl.constant 3 : !nsl.bits<8>
  %a = nsl.constant 3 : !nsl.bits<8>
  // CHECK: %{{.*}} = nsl.constant 4 : !nsl.bits<8>
  %b = nsl.constant 4 : !nsl.bits<8>
  // CHECK: %{{.*}} = nsl.mul %{{.*}}, %{{.*}} : !nsl.bits<8>
  %prod = nsl.mul %a, %b : !nsl.bits<8>
  // CHECK: nsl.wire "dst" : !nsl.bits<8>
  %dst = nsl.wire "dst" : !nsl.bits<8>
  // CHECK: nsl.transfer %{{.*}}, %{{.*}} : !nsl.bits<8>
  nsl.transfer %dst, %prod : !nsl.bits<8>
}
