// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// RUN: nsl-opt %s | FileCheck %s
// RUN: nsl-opt %s | nsl-opt - | FileCheck %s
//
// FR-010 round-trip for `nsl.field_decl` (per Q6 Option B, Session
// 2026-04-30 — split from `nsl.field`'s overloaded role). Form:
// `nsl.field_decl "name" : !nsl.bits<N>` inside an `nsl.struct` body.
// Symbol trait + parent = `nsl.struct` per FR-013 (post-Q6).

// CHECK-LABEL: nsl.module @FieldDeclHost
nsl.module @FieldDeclHost {
  // CHECK: nsl.struct @S
  nsl.struct @S {
    // CHECK: nsl.field_decl "a" : !nsl.bits<4>
    nsl.field_decl "a" : !nsl.bits<4>
    // CHECK: nsl.field_decl "b" : !nsl.bits<12>
    nsl.field_decl "b" : !nsl.bits<12>
  }
}
