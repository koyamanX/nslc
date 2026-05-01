// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// RUN: nsl-opt %s | FileCheck %s
// RUN: nsl-opt %s | nsl-opt - | FileCheck %s
//
// Post-merge M4-amendment 2026-05-02 (Phase A cluster 1+3+4): round-trip
// for `nsl.shl` (Pure + SameOperandsAndResultType; NSL `<<` per EBNF §11
// line 620). Lowers to `comb.shl` at M6 per design §10.

// CHECK-LABEL: nsl.module @ShlHost
nsl.module @ShlHost {
  // CHECK: %{{.*}} = nsl.constant 1 : !nsl.bits<8>
  %a = nsl.constant 1 : !nsl.bits<8>
  // CHECK: %{{.*}} = nsl.constant 4 : !nsl.bits<8>
  %b = nsl.constant 4 : !nsl.bits<8>
  // CHECK: %{{.*}} = nsl.shl %{{.*}}, %{{.*}} : !nsl.bits<8>
  %r = nsl.shl %a, %b : !nsl.bits<8>
  // CHECK: nsl.wire "dst" : !nsl.bits<8>
  %dst = nsl.wire "dst" : !nsl.bits<8>
  // CHECK: nsl.transfer %{{.*}}, %{{.*}} : !nsl.bits<8>
  nsl.transfer %dst, %r : !nsl.bits<8>
}
