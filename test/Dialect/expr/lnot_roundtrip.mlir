// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// RUN: nsl-opt %s | FileCheck %s
// RUN: nsl-opt %s | nsl-opt - | FileCheck %s
//
// Post-merge M4-amendment 2026-05-02 (Phase A cluster 5): round-trip
// for `nsl.lnot` (Pure; NSL `!` per EBNF §11 line 627). Both operand
// and result MUST be `!nsl.bits<1>`. Lowers to `comb.icmp eq %a, 0`.

// CHECK-LABEL: nsl.module @LnotHost
nsl.module @LnotHost {
  // CHECK: %{{.*}} = nsl.constant 1 : !nsl.bits<1>
  %a = nsl.constant 1 : !nsl.bits<1>
  // CHECK: %{{.*}} = nsl.lnot %{{.*}} : !nsl.bits<1> -> !nsl.bits<1>
  %r = nsl.lnot %a : !nsl.bits<1> -> !nsl.bits<1>
  // CHECK: nsl.wire "dst" : !nsl.bits<1>
  %dst = nsl.wire "dst" : !nsl.bits<1>
  // CHECK: nsl.transfer %{{.*}}, %{{.*}} : !nsl.bits<1>
  nsl.transfer %dst, %r : !nsl.bits<1>
}
