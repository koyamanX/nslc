// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// RUN: nsl-opt %s | FileCheck %s
// RUN: nsl-opt %s | nsl-opt - | FileCheck %s
//
// Post-merge M4-amendment 2026-05-02 (Phase A cluster 7b): round-trip
// for `nsl.repeat` (Pure; NSL `N{a}` per EBNF §11 line 700; spec
// S15-style compile-time count). Lowers to `comb.replicate` at M6.

// CHECK-LABEL: nsl.module @RepeatHost
nsl.module @RepeatHost {
  // CHECK: %{{.*}} = nsl.constant 5 : !nsl.bits<3>
  %a = nsl.constant 5 : !nsl.bits<3>
  // CHECK: %{{.*}} = nsl.repeat %{{.*}}, 4 : !nsl.bits<3> -> !nsl.bits<12>
  %r = nsl.repeat %a, 4 : !nsl.bits<3> -> !nsl.bits<12>
  // CHECK: nsl.wire "dst" : !nsl.bits<12>
  %dst = nsl.wire "dst" : !nsl.bits<12>
  // CHECK: nsl.transfer %{{.*}}, %{{.*}} : !nsl.bits<12>
  nsl.transfer %dst, %r : !nsl.bits<12>
}
