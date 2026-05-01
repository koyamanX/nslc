// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// RUN: nsl-opt %s | FileCheck %s
// RUN: nsl-opt %s | nsl-opt - | FileCheck %s
//
// Post-merge M4-amendment 2026-05-02 (Phase A cluster 7b): round-trip
// for `nsl.extract` (Pure; NSL `v[hi:lo]` / `v[i]` per EBNF §11 lines
// 639–640; spec S15 mandates compile-time-constant indices). Lowers to
// `comb.extract` at M6 per design §10.

// CHECK-LABEL: nsl.module @ExtractHost
nsl.module @ExtractHost {
  // CHECK: %{{.*}} = nsl.constant 255 : !nsl.bits<8>
  %v = nsl.constant 255 : !nsl.bits<8>
  // CHECK: %{{.*}} = nsl.extract %{{.*}}, 2 : !nsl.bits<8> -> !nsl.bits<3>
  %r = nsl.extract %v, 2 : !nsl.bits<8> -> !nsl.bits<3>
  // CHECK: nsl.wire "dst" : !nsl.bits<3>
  %dst = nsl.wire "dst" : !nsl.bits<3>
  // CHECK: nsl.transfer %{{.*}}, %{{.*}} : !nsl.bits<3>
  nsl.transfer %dst, %r : !nsl.bits<3>
}
