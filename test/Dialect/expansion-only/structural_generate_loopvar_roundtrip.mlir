// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// RUN: nsl-opt %s | FileCheck %s
// RUN: nsl-opt %s | nsl-opt - | FileCheck %s
//
// Post-merge M4-amendment 2026-05-02 (#4): round-trip for
// `nsl.structural_generate` carrying the OPTIONAL `loop_var`
// `StrAttr`. M5's `NSLExpandGeneratePass` reads this attribute to
// know which `%IDENT%` macro residue to substitute when materialising
// per-iteration body copies. The existing
// `structural_generate_roundtrip.mlir` covers the empty-loop_var
// (backward-compat) path; this fixture covers the present-loop_var
// path.
//
// Attribute order is alphabetized by the printer: loop_var < lower <
// step < upper.

// CHECK-LABEL: nsl.module @GenLoopVarHost
nsl.module @GenLoopVarHost {
  // CHECK: nsl.structural_generate attributes {loop_var = "i", lower = 0 : i64, step = 1 : i64, upper = 4 : i64}
  nsl.structural_generate attributes {lower = 0 : i64, upper = 4 : i64, step = 1 : i64, loop_var = "i"} {
  }
  // CHECK: nsl.structural_generate attributes {loop_var = "j", lower = 0 : i64, step = 2 : i64, upper = 16 : i64}
  nsl.structural_generate attributes {lower = 0 : i64, upper = 16 : i64, step = 2 : i64, loop_var = "j"} {
  }
}
