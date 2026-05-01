// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// RUN: nsl-opt %s | FileCheck %s
// RUN: nsl-opt %s | nsl-opt - | FileCheck %s
//
// Post-merge M4-amendment 2026-05-02 (Phase A cluster 5): round-trip
// for `nsl.reduce_xor` (parity). Lowers to `comb.parity` at M6.

// CHECK-LABEL: nsl.module @ReduceXorHost
nsl.module @ReduceXorHost {
  // CHECK: %{{.*}} = nsl.constant 7 : !nsl.bits<8>
  %a = nsl.constant 7 : !nsl.bits<8>
  // CHECK: %{{.*}} = nsl.reduce_xor %{{.*}} : !nsl.bits<8> -> !nsl.bits<1>
  %r = nsl.reduce_xor %a : !nsl.bits<8> -> !nsl.bits<1>
  // CHECK: nsl.wire "dst" : !nsl.bits<1>
  %dst = nsl.wire "dst" : !nsl.bits<1>
  // CHECK: nsl.transfer %{{.*}}, %{{.*}} : !nsl.bits<1>
  nsl.transfer %dst, %r : !nsl.bits<1>
}
