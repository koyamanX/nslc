// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// RUN: nsl-opt %s | FileCheck %s
// RUN: nsl-opt %s | nsl-opt - | FileCheck %s
//
// FR-018 type round-trip: `!nsl.mem<[D x T]>` over representative
// shapes (bits-element + struct-element). Two-pass per FR-017.

// CHECK-LABEL: nsl.module @mem_typed
nsl.module @mem_typed {
  // CHECK: nsl.struct @Word
  nsl.struct @Word {
    nsl.field_decl "a" : !nsl.bits<8>
    nsl.field_decl "b" : !nsl.bits<8>
  }
  // CHECK: %{{.*}} = nsl.mem "ram" : !nsl.mem<[256 x !nsl.bits<8>]>
  %ram = nsl.mem "ram" : !nsl.mem<[256 x !nsl.bits<8>]>
  // CHECK: %{{.*}} = nsl.mem "rec_ram" : !nsl.mem<[16 x !nsl.struct<@Word>]>
  %rec_ram = nsl.mem "rec_ram" : !nsl.mem<[16 x !nsl.struct<@Word>]>
}
