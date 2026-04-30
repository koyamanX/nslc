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

// Phase 4 SYN-6: `attr-dict-with-keyword` requires the `attributes`
// keyword before the inline-attr brace block, otherwise the parser
// reads `{...}` as the body region. Attribute order is alphabetized
// by the printer (`lower`, `step`, `upper`).

// CHECK-LABEL: nsl.module @GenHost
nsl.module @GenHost {
  // CHECK: nsl.structural_generate attributes {lower = 0 : i64, step = 1 : i64, upper = 8 : i64}
  nsl.structural_generate attributes {lower = 0 : i64, upper = 8 : i64, step = 1 : i64} {
  }
}
