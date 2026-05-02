// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// RUN: nsl-opt %s | FileCheck %s
// RUN: nsl-opt %s | nsl-opt - | FileCheck %s
//
// Post-merge M4-amendment 2026-05-02 (Phase A cluster 1+3+4): round-trip
// for `nsl.sub` (Pure + SameOperandsAndResultType; NSL `-` binary per
// EBNF §11 line 622). Lowers to `comb.sub` at M6 per design §10.

// CHECK-LABEL: nsl.module @SubHost
nsl.module @SubHost {
  // CHECK: %{{.*}} = nsl.constant 5 : !nsl.bits<8>
  %a = nsl.constant 5 : !nsl.bits<8>
  // CHECK: %{{.*}} = nsl.constant 2 : !nsl.bits<8>
  %b = nsl.constant 2 : !nsl.bits<8>
  // CHECK: %{{.*}} = nsl.sub %{{.*}}, %{{.*}} : !nsl.bits<8>
  %diff = nsl.sub %a, %b : !nsl.bits<8>
  // CHECK: nsl.wire "dst" : !nsl.bits<8>
  %dst = nsl.wire "dst" : !nsl.bits<8>
  // CHECK: nsl.transfer %{{.*}}, %{{.*}} : !nsl.bits<8>
  nsl.transfer %dst, %diff : !nsl.bits<8>
}
