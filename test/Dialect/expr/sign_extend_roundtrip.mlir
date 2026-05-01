// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// RUN: nsl-opt %s | FileCheck %s
// RUN: nsl-opt %s | nsl-opt - | FileCheck %s
//
// Post-merge M4-amendment 2026-05-02 (Phase A cluster 6): round-trip
// for `nsl.sign_extend` (Pure; NSL `N#expr` / `N#(expr)` per EBNF §11
// lines 702–703).

// CHECK-LABEL: nsl.module @SignExtHost
nsl.module @SignExtHost {
  // CHECK: %{{.*}} = nsl.constant 5 : !nsl.bits<8>
  %a = nsl.constant 5 : !nsl.bits<8>
  // CHECK: %{{.*}} = nsl.sign_extend %{{.*}} : !nsl.bits<8> to !nsl.bits<16>
  %r = nsl.sign_extend %a : !nsl.bits<8> to !nsl.bits<16>
  // CHECK: nsl.wire "dst" : !nsl.bits<16>
  %dst = nsl.wire "dst" : !nsl.bits<16>
  // CHECK: nsl.transfer %{{.*}}, %{{.*}} : !nsl.bits<16>
  nsl.transfer %dst, %r : !nsl.bits<16>
}
