// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// RUN: nsl-opt %s | FileCheck %s
// RUN: nsl-opt %s | nsl-opt - | FileCheck %s
//
// FR-010 round-trip for `nsl.incdec` (design §7 line 914). Form:
// `nsl.incdec %reg { kind = pre_inc }`. Kind enum per FR-013.
//
// NOTE for Phase 4: the exact kind enum text form ("pre_inc",
// "post_inc", "pre_dec", "post_dec") is not pinned in the spec.
// Picked the C-style names; flag for Phase 4 review when the
// `IncDecKind` enum-attr is materialized.

// Phase 4 SYN-5: `I64Attr` prints just `0` (no `: i64`).

// CHECK-LABEL: nsl.module @IncDecHost
nsl.module @IncDecHost {
  // CHECK: %{{.*}} = nsl.reg "q" : !nsl.bits<8> = 0
  %q = nsl.reg "q" : !nsl.bits<8> = 0
  // CHECK: nsl.incdec %{{.*}} : !nsl.bits<8> {kind = #nsl<incdec_kind pre_inc>}
  nsl.incdec %q : !nsl.bits<8> {kind = #nsl<incdec_kind pre_inc>}
  // CHECK: nsl.incdec %{{.*}} : !nsl.bits<8> {kind = #nsl<incdec_kind post_dec>}
  nsl.incdec %q : !nsl.bits<8> {kind = #nsl<incdec_kind post_dec>}
}
