// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// RUN: nsl-opt %s | FileCheck %s
// RUN: nsl-opt %s | nsl-opt - | FileCheck %s
//
// Post-merge M4-amendment 2026-05-02 (Phase A cluster 7a): round-trip
// for `nsl.mux` (Pure; NSL `if (c) a else b` per EBNF §11 lines 601–604;
// S14 mandates the else-branch in expressions). Lowers to `comb.mux` at
// M6.

// CHECK-LABEL: nsl.module @MuxHost
nsl.module @MuxHost {
  // CHECK: %{{.*}} = nsl.constant 1 : !nsl.bits<1>
  %c = nsl.constant 1 : !nsl.bits<1>
  // CHECK: %{{.*}} = nsl.constant 5 : !nsl.bits<8>
  %a = nsl.constant 5 : !nsl.bits<8>
  // CHECK: %{{.*}} = nsl.constant 9 : !nsl.bits<8>
  %b = nsl.constant 9 : !nsl.bits<8>
  // CHECK: %{{.*}} = nsl.mux %{{.*}}, %{{.*}}, %{{.*}} : !nsl.bits<1>, !nsl.bits<8>, !nsl.bits<8> -> !nsl.bits<8>
  %r = nsl.mux %c, %a, %b : !nsl.bits<1>, !nsl.bits<8>, !nsl.bits<8> -> !nsl.bits<8>
  // CHECK: nsl.wire "dst" : !nsl.bits<8>
  %dst = nsl.wire "dst" : !nsl.bits<8>
  // CHECK: nsl.transfer %{{.*}}, %{{.*}} : !nsl.bits<8>
  nsl.transfer %dst, %r : !nsl.bits<8>
}
