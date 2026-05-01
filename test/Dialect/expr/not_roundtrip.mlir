// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// RUN: nsl-opt %s | FileCheck %s
// RUN: nsl-opt %s | nsl-opt - | FileCheck %s
//
// Post-merge M4-amendment 2026-05-02 (Phase A cluster 5): round-trip
// for `nsl.not` (Pure + SameOperandsAndResultType; NSL `~` per EBNF
// §11 line 626). Lowers to `comb.xor %a, all-ones` at M6.

// CHECK-LABEL: nsl.module @NotHost
nsl.module @NotHost {
  // CHECK: %{{.*}} = nsl.constant 15 : !nsl.bits<8>
  %a = nsl.constant 15 : !nsl.bits<8>
  // CHECK: %{{.*}} = nsl.not %{{.*}} : !nsl.bits<8>
  %r = nsl.not %a : !nsl.bits<8>
  // CHECK: nsl.wire "dst" : !nsl.bits<8>
  %dst = nsl.wire "dst" : !nsl.bits<8>
  // CHECK: nsl.transfer %{{.*}}, %{{.*}} : !nsl.bits<8>
  nsl.transfer %dst, %r : !nsl.bits<8>
}
