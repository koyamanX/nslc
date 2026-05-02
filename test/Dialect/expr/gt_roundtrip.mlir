// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// RUN: nsl-opt %s | FileCheck %s
// RUN: nsl-opt %s | nsl-opt - | FileCheck %s
//
// Post-merge M4-amendment 2026-05-02 (Phase A cluster 2): round-trip
// for `nsl.gt` (NSL `>` per EBNF §11 line 618). Lowers to
// `comb.icmp ugt` at M6.

// CHECK-LABEL: nsl.module @GtHost
nsl.module @GtHost {
  // CHECK: %{{.*}} = nsl.constant 5 : !nsl.bits<8>
  %a = nsl.constant 5 : !nsl.bits<8>
  // CHECK: %{{.*}} = nsl.constant 2 : !nsl.bits<8>
  %b = nsl.constant 2 : !nsl.bits<8>
  // CHECK: %{{.*}} = nsl.gt %{{.*}}, %{{.*}} : !nsl.bits<8> -> !nsl.bits<1>
  %r = nsl.gt %a, %b : !nsl.bits<8> -> !nsl.bits<1>
  // CHECK: nsl.wire "dst" : !nsl.bits<1>
  %dst = nsl.wire "dst" : !nsl.bits<1>
  // CHECK: nsl.transfer %{{.*}}, %{{.*}} : !nsl.bits<1>
  nsl.transfer %dst, %r : !nsl.bits<1>
}
