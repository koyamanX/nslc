// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// RUN: nsl-opt %s | FileCheck %s
// RUN: nsl-opt %s | nsl-opt - | FileCheck %s
//
// FR-010 round-trip for `nsl.struct` (module-level structural type
// declaration; design §7 line 887). Multiple `nsl.field` children
// with mixed bit-widths.

// CHECK-LABEL: nsl.module @StructHost
nsl.module @StructHost {
  // Per Q6 Option B: in-struct-body field declarations use the new
  // `nsl.field_decl` op (the access-marker form `nsl.field` is split
  // off into the marker category — see `marker/field_roundtrip.mlir`).
  // CHECK: nsl.struct @Pair
  nsl.struct @Pair {
    // CHECK: nsl.field_decl "lo" : !nsl.bits<4>
    nsl.field_decl "lo" : !nsl.bits<4>
    // CHECK: nsl.field_decl "hi" : !nsl.bits<4>
    nsl.field_decl "hi" : !nsl.bits<4>
  }
  // CHECK: nsl.struct @Wide
  nsl.struct @Wide {
    // CHECK: nsl.field_decl "a" : !nsl.bits<32>
    nsl.field_decl "a" : !nsl.bits<32>
    // CHECK: nsl.field_decl "b" : !nsl.bits<32>
    nsl.field_decl "b" : !nsl.bits<32>
    // CHECK: nsl.field_decl "c" : !nsl.bits<32>
    nsl.field_decl "c" : !nsl.bits<32>
  }
}
