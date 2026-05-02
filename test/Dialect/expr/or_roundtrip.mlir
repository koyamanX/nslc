// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// RUN: nsl-opt %s | FileCheck %s
// RUN: nsl-opt %s | nsl-opt - | FileCheck %s
//
// Post-merge M4-amendment 2026-05-02 (Phase A cluster 1+3+4): round-trip
// for `nsl.or` (Pure + SameOperandsAndResultType + Commutative; NSL `|`
// binary per EBNF §11 line 610, disambiguated from reduction by N2).
// Lowers to `comb.or` at M6 per design §10.

// CHECK-LABEL: nsl.module @OrHost
nsl.module @OrHost {
  // CHECK: %{{.*}} = nsl.constant 15 : !nsl.bits<8>
  %a = nsl.constant 15 : !nsl.bits<8>
  // CHECK: %{{.*}} = nsl.constant 240 : !nsl.bits<8>
  %b = nsl.constant 240 : !nsl.bits<8>
  // CHECK: %{{.*}} = nsl.or %{{.*}}, %{{.*}} : !nsl.bits<8>
  %r = nsl.or %a, %b : !nsl.bits<8>
  // CHECK: nsl.wire "dst" : !nsl.bits<8>
  %dst = nsl.wire "dst" : !nsl.bits<8>
  // CHECK: nsl.transfer %{{.*}}, %{{.*}} : !nsl.bits<8>
  nsl.transfer %dst, %r : !nsl.bits<8>
}
