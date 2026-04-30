// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// RUN: nsl-opt %s | FileCheck %s
// RUN: nsl-opt %s | nsl-opt - | FileCheck %s
//
// FR-018 type round-trip: `!nsl.bits<N>` for N in {1, 8, 16, 64} on
// `nsl.wire` (the storage op whose type slot is per FR-013 statically
// `!nsl.bits<N>`). Two-pass round-trip per FR-017 + stability §5.

// CHECK-LABEL: nsl.module @bits_widths
nsl.module @bits_widths {
  // CHECK: %{{.*}} = nsl.wire "w1" : !nsl.bits<1>
  %w1 = nsl.wire "w1" : !nsl.bits<1>
  // CHECK: %{{.*}} = nsl.wire "w8" : !nsl.bits<8>
  %w8 = nsl.wire "w8" : !nsl.bits<8>
  // CHECK: %{{.*}} = nsl.wire "w16" : !nsl.bits<16>
  %w16 = nsl.wire "w16" : !nsl.bits<16>
  // CHECK: %{{.*}} = nsl.wire "w64" : !nsl.bits<64>
  %w64 = nsl.wire "w64" : !nsl.bits<64>
}
