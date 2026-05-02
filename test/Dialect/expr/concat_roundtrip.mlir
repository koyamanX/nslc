// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// RUN: nsl-opt %s | FileCheck %s
// RUN: nsl-opt %s | nsl-opt - | FileCheck %s
//
// Post-merge M4-amendment 2026-05-02 (Phase A cluster 7a): round-trip
// for `nsl.concat` (Pure; NSL `{a, b, ...}` per EBNF §11 line 698;
// MSB-first per S18 packing). Lowers to `comb.concat` (variadic) at M6.

// CHECK-LABEL: nsl.module @ConcatHost
nsl.module @ConcatHost {
  // CHECK: %{{.*}} = nsl.constant 0 : !nsl.bits<3>
  %a = nsl.constant 0 : !nsl.bits<3>
  // CHECK: %{{.*}} = nsl.constant 5 : !nsl.bits<5>
  %b = nsl.constant 5 : !nsl.bits<5>
  // CHECK: %{{.*}} = nsl.concat %{{.*}}, %{{.*}} : (!nsl.bits<3>, !nsl.bits<5>) -> !nsl.bits<8>
  %r = nsl.concat %a, %b : (!nsl.bits<3>, !nsl.bits<5>) -> !nsl.bits<8>
  // CHECK: nsl.wire "dst" : !nsl.bits<8>
  %dst = nsl.wire "dst" : !nsl.bits<8>
  // CHECK: nsl.transfer %{{.*}}, %{{.*}} : !nsl.bits<8>
  nsl.transfer %dst, %r : !nsl.bits<8>
}
