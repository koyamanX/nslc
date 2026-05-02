// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// RUN: nsl-opt %s | FileCheck %s
// RUN: nsl-opt %s | nsl-opt - | FileCheck %s
//
// Post-merge M4-amendment 2026-05-02 (Phase A cluster 2): round-trip
// for `nsl.ge` (NSL `>=` per EBNF §11 line 618). Lowers to
// `comb.icmp uge` at M6.

// CHECK-LABEL: nsl.module @GeHost
nsl.module @GeHost {
  // CHECK: %{{.*}} = nsl.constant 5 : !nsl.bits<8>
  %a = nsl.constant 5 : !nsl.bits<8>
  // CHECK: %{{.*}} = nsl.constant 5 : !nsl.bits<8>
  %b = nsl.constant 5 : !nsl.bits<8>
  // CHECK: %{{.*}} = nsl.ge %{{.*}}, %{{.*}} : !nsl.bits<8> -> !nsl.bits<1>
  %r = nsl.ge %a, %b : !nsl.bits<8> -> !nsl.bits<1>
  // CHECK: nsl.wire "dst" : !nsl.bits<1>
  %dst = nsl.wire "dst" : !nsl.bits<1>
  // CHECK: nsl.transfer %{{.*}}, %{{.*}} : !nsl.bits<1>
  nsl.transfer %dst, %r : !nsl.bits<1>
}
