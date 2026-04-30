// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// RUN: nsl-opt %s | FileCheck %s
// RUN: nsl-opt %s | nsl-opt - | FileCheck %s
//
// FR-010 round-trip for `nsl.field` (design §8 line 1061). Two
// roles per the design + FR-013:
//   - Module-level: declares a field inside an `nsl.struct` body.
//   - Marker (lowering helper): struct field access carrying an
//     integer attr for the field index.
// The struct-body declaration form is exercised here via the
// surrounding nsl.struct; the access form is the marker shape.
//
// NOTE for Phase 4: the spec doesn't pin the exact text form for
// the field-access marker (member name vs. integer index). Picked
// the integer-index form per data-model §2.10 ("int-attr field
// index"); flag for Phase 4 if the access syntax changes.

// CHECK-LABEL: nsl.module @FieldHost
nsl.module @FieldHost {
  // CHECK: nsl.struct @Pair
  nsl.struct @Pair {
    // CHECK: nsl.field "lo" : !nsl.bits<8>
    nsl.field "lo" : !nsl.bits<8>
    // CHECK: nsl.field "hi" : !nsl.bits<8>
    nsl.field "hi" : !nsl.bits<8>
  }
  // CHECK: nsl.reg "r" : !nsl.struct<@Pair>
  %r = nsl.reg "r" : !nsl.struct<@Pair>
  // Field-access marker form with int index.
  // CHECK: nsl.field %{{.*}} {index = 0 : i64} : !nsl.struct<@Pair> -> !nsl.bits<8>
  %lo = nsl.field %r {index = 0 : i64} : !nsl.struct<@Pair> -> !nsl.bits<8>
}
