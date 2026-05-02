// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// RUN: nsl-opt %s | FileCheck %s
// RUN: nsl-opt %s | nsl-opt - | FileCheck %s
//
// Post-merge M4-amendment 2026-05-02 (Phase A cluster 5): round-trip
// for `nsl.reduce_and` (Pure; NSL `&` prefix reduction per EBNF §11
// line 634). Operand `!nsl.bits<N>`, result `!nsl.bits<1>`. Lowers to
// `comb.icmp eq %a, all-ones` at M6.

// CHECK-LABEL: nsl.module @ReduceAndHost
nsl.module @ReduceAndHost {
  // CHECK: %{{.*}} = nsl.constant 255 : !nsl.bits<8>
  %a = nsl.constant 255 : !nsl.bits<8>
  // CHECK: %{{.*}} = nsl.reduce_and %{{.*}} : !nsl.bits<8> -> !nsl.bits<1>
  %r = nsl.reduce_and %a : !nsl.bits<8> -> !nsl.bits<1>
  // CHECK: nsl.wire "dst" : !nsl.bits<1>
  %dst = nsl.wire "dst" : !nsl.bits<1>
  // CHECK: nsl.transfer %{{.*}}, %{{.*}} : !nsl.bits<1>
  nsl.transfer %dst, %r : !nsl.bits<1>
}
