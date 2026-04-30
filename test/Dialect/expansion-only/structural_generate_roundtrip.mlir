// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// RUN: nsl-opt %s | FileCheck %s
// RUN: nsl-opt %s | nsl-opt - | FileCheck %s
//
// FR-010 round-trip for `nsl.structural_generate` (design §9 line
// 1073). One region; consumed by M5's `NSLExpandGeneratePass`. Per
// FR-013 + data-model §2.11, the loop-bound attrs shape is
// verifier-checked.
//
// NOTE for Phase 4: the spec doesn't pin the exact attribute names
// for loop bounds. Picked descriptive `lower`/`upper`/`step` int
// attrs; flag for Phase 4 review when the attr set is materialized.

// CHECK-LABEL: nsl.module @GenHost
nsl.module @GenHost {
  // CHECK: nsl.structural_generate
  nsl.structural_generate {lower = 0 : i64, upper = 8 : i64, step = 1 : i64} {
  }
}
