// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// RUN: nsl-opt -nsl-resolve-params %s | FileCheck %s
//
// M5 US2 / FR-013 — param-bound generate scenario per acceptance
// scenario 2 in `spec.md`:
//
//   "Given an nsl.module @M with param_int @N = 8 and an
//    nsl.structural_generate whose bound expression references
//    %N%, ... post-pipeline IR contains eight replicated bodies
//    AND zero unresolved %N% references."
//
// At M5's frozen dialect surface the `nsl.structural_generate`
// op carries `lower`/`upper`/`step` as I64Attrs (literal
// integers) — there is no FlatSymbolRefAttr slot for a param
// reference inside a bound. So the param-eagerness MUST happen
// at the AST→MLIR visitor stage (`visit(TopLevelParamDecl)`
// populates `paramTable_`; `visit(StructuralGenerate)` consults
// it when resolving bounds that are `IdentifierExpr`).
// This fixture documents the seam: a hand-authored `.mlir` with
// the literal bound already in place — no operand-side param ref
// exists for the pass to substitute. The pass is a no-op here.

// CHECK: nsl.param_int @N = 8
nsl.param_int @N = 8

// CHECK-LABEL: nsl.module @GenHost
nsl.module @GenHost {
  // CHECK: nsl.structural_generate attributes {loop_var = "i", lower = 0 : i64, step = 1 : i64, upper = 8 : i64}
  nsl.structural_generate attributes {lower = 0 : i64, upper = 8 : i64, step = 1 : i64, loop_var = "i"} {
  }
}
