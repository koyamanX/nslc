// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// RUN: nsl-opt %s | FileCheck %s
// RUN: nsl-opt %s | nsl-opt - | FileCheck %s
//
// Post-merge M4-amendment 2026-05-02 (Phase A cluster 2): round-trip
// for `nsl.eq` (Pure + SameTypeOperands + Commutative; NSL `==` per
// EBNF §11 line 616). Result type is always `!nsl.bits<1>`. Lowers to
// `comb.icmp eq` at M6 per design §10.

// CHECK-LABEL: nsl.module @EqHost
nsl.module @EqHost {
  // CHECK: %{{.*}} = nsl.constant 1 : !nsl.bits<8>
  %a = nsl.constant 1 : !nsl.bits<8>
  // CHECK: %{{.*}} = nsl.constant 2 : !nsl.bits<8>
  %b = nsl.constant 2 : !nsl.bits<8>
  // CHECK: %{{.*}} = nsl.eq %{{.*}}, %{{.*}} : !nsl.bits<8> -> !nsl.bits<1>
  %r = nsl.eq %a, %b : !nsl.bits<8> -> !nsl.bits<1>
  // CHECK: nsl.wire "dst" : !nsl.bits<1>
  %dst = nsl.wire "dst" : !nsl.bits<1>
  // CHECK: nsl.transfer %{{.*}}, %{{.*}} : !nsl.bits<1>
  nsl.transfer %dst, %r : !nsl.bits<1>
}
