// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// RUN: nsl-opt --mlir-very-unsafe-disable-verifier-on-parsing -nsl-expand-generate %s | FileCheck %s
//
// M5 US2 / FR-014 — `NSLExpandGeneratePass` (slot 2). Acceptance
// scenario 1 (`spec.md:228`):
//
//   "Given an nsl.module @M containing a single nsl.structural_
//    generate with literal bounds 0..4 and a body declaring nsl.reg
//    'buf_%i%' : !nsl.bits<8>, when NSLExpandGeneratePass runs,
//    then the post-pass IR contains four nsl.reg ops named 'buf_0',
//    'buf_1', 'buf_2', 'buf_3' AND zero nsl.structural_generate."
//
// Expansion rule: clone the body region N times (N = (upper - lower
// + step - 1) / step), with each clone having its `%<loop_var>%`
// substring inside every `StringAttr` value replaced by the per-
// iteration integer (decimal). The original op is erased.
//
// M4 DESIGN TENSION (offload Commit 3): `nsl.reg`'s parent
// constraint is `ParentOneOf<["ModuleOp", "ProcOp"]>` (NSLOps.td
// line 243). When `nsl.reg` appears inside `nsl.structural_generate`'s
// body — the *natural* hand-authored shape per FR-014 — the
// immediate parent is `nsl.structural_generate`, which the verifier
// rejects pre-expansion. Post-expansion the body ops land directly
// under `nsl.module` (the structural_generate's parent), which IS
// verifier-clean. The `--mlir-very-unsafe-disable-verifier-on-parsing`
// flag lets the input parse; the pass output is then re-verified by
// MLIR's pass infrastructure (so the post-expansion IR's
// verifier-cleanliness is still asserted via `--verify-each`'s
// implicit-on default + the FileCheck assertions below). A future
// M4 amendment may relax `nsl.reg`'s parent constraint to accept
// `StructuralGenerateOp` so this flag can be dropped.

// CHECK-LABEL: nsl.module @GenLiteral
// CHECK-NOT: nsl.structural_generate
nsl.module @GenLiteral {
  // CHECK: nsl.reg "buf_0" : !nsl.bits<8>
  // CHECK: nsl.reg "buf_1" : !nsl.bits<8>
  // CHECK: nsl.reg "buf_2" : !nsl.bits<8>
  // CHECK: nsl.reg "buf_3" : !nsl.bits<8>
  nsl.structural_generate attributes {lower = 0 : i64, upper = 4 : i64, step = 1 : i64, loop_var = "i"} {
    nsl.reg "buf_%i%" : !nsl.bits<8>
  }
}
