// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// RUN: nsl-opt %s | FileCheck %s
// RUN: nsl-opt %s | nsl-opt - | FileCheck %s
//
// FR-010 round-trip for `nsl.struct_cast` (design §8 line 1061).
// Bits → struct preserve. Per Q3 Option A, this is the explicit op
// that M5 lowering inserts at every user-written struct↔bits
// conversion site (the dialect verifier never tolerates implicit
// reinterpretation).

// CHECK-LABEL: nsl.module @StructCastHost
nsl.module @StructCastHost {
  // Per Q6 Option B: struct-internal field decls use `nsl.field_decl`.
  // CHECK: nsl.struct @S
  nsl.struct @S {
    nsl.field_decl "lo" : !nsl.bits<8>
    nsl.field_decl "hi" : !nsl.bits<8>
  }
  // CHECK: %{{.*}} = nsl.wire "raw" : !nsl.bits<16>
  %raw = nsl.wire "raw" : !nsl.bits<16>
  // CHECK: %{{.*}} = nsl.struct_cast %{{.*}} : !nsl.bits<16> to !nsl.struct<@S>
  %rec = nsl.struct_cast %raw : !nsl.bits<16> to !nsl.struct<@S>
}
