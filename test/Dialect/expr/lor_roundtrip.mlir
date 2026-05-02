// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// RUN: nsl-opt %s | FileCheck %s
// RUN: nsl-opt %s | nsl-opt - | FileCheck %s
//
// Post-merge M4-amendment 2026-05-02 (Phase A cluster 2): round-trip
// for `nsl.lor` (NSL `||` per EBNF §11 line 606). Both operands and
// the result MUST be `!nsl.bits<1>`. Lowers to `comb.or` (on width-1
// operands) at M6.

// CHECK-LABEL: nsl.module @LorHost
nsl.module @LorHost {
  // CHECK: %{{.*}} = nsl.constant 1 : !nsl.bits<1>
  %a = nsl.constant 1 : !nsl.bits<1>
  // CHECK: %{{.*}} = nsl.constant 0 : !nsl.bits<1>
  %b = nsl.constant 0 : !nsl.bits<1>
  // CHECK: %{{.*}} = nsl.lor %{{.*}}, %{{.*}} : !nsl.bits<1> -> !nsl.bits<1>
  %r = nsl.lor %a, %b : !nsl.bits<1> -> !nsl.bits<1>
  // CHECK: nsl.wire "dst" : !nsl.bits<1>
  %dst = nsl.wire "dst" : !nsl.bits<1>
  // CHECK: nsl.transfer %{{.*}}, %{{.*}} : !nsl.bits<1>
  nsl.transfer %dst, %r : !nsl.bits<1>
}
