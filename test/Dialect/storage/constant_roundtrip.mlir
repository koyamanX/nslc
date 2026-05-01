// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// RUN: nsl-opt %s | FileCheck %s
// RUN: nsl-opt %s | nsl-opt - | FileCheck %s
//
// Post-merge M4-amendment 2026-05-01: round-trip for `nsl.constant`
// (Pure + ConstantLike value-producer feeding bits-typed operands of
// `NSL_BitsOrStruct`-constrained ops; closes the M5 gap surfaced by
// the four-way decision option (a)). Form:
// `nsl.constant <value> : !nsl.bits<N>`.

// CHECK-LABEL: nsl.module @ConstHost
nsl.module @ConstHost {
  // CHECK: %{{.*}} = nsl.constant 0 : !nsl.bits<8>
  %c0 = nsl.constant 0 : !nsl.bits<8>
  // CHECK: %{{.*}} = nsl.constant 255 : !nsl.bits<8>
  %cmax = nsl.constant 255 : !nsl.bits<8>
  // CHECK: %{{.*}} = nsl.constant 1 : !nsl.bits<1>
  %c1bit = nsl.constant 1 : !nsl.bits<1>
  // CHECK: %{{.*}} = nsl.constant 4096 : !nsl.bits<32>
  %cwide = nsl.constant 4096 : !nsl.bits<32>
  // CHECK: nsl.wire "dst" : !nsl.bits<8>
  %dst = nsl.wire "dst" : !nsl.bits<8>
  // CHECK: nsl.transfer %{{.*}}, %{{.*}} : !nsl.bits<8>
  nsl.transfer %dst, %cmax : !nsl.bits<8>
}
