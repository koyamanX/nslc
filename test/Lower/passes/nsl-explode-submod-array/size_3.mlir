// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// RUN: nsl-opt -nsl-explode-submod-array %s | FileCheck %s
//
// M5 US4 / FR-016 — `NSLExplodeSubmodArrayPass` (slot 4 of the
// 6-slot pipeline per `pass-pipeline.contract.md` §2 row 4).
// Acceptance scenario (`spec.md` US4 + `data-model.md` §7.2):
//
//   "Given an `nsl.module` with `nsl.submodule @inst : @SUB[3]`,
//    when NSLExplodeSubmodArrayPass runs, then the post-pass IR
//    contains exactly three independent `nsl.submodule` ops named
//    @inst_0, @inst_1, @inst_2 (each of singleton form @SUB) AND
//    zero array-form `nsl.submodule`."
//
// Naming scheme: `<orig-name>_<index>`. Per offload guidance, we
// avoid the `[]` symbol-name surface (MLIR symbol names cannot
// contain unescaped `[`/`]`) — the in-source-printer notation
// `@inst[3]` is a textual decoration of the OPTIONAL array_size
// attribute, not the canonical symbol name.

// CHECK-LABEL: nsl.module @TopArr3
// CHECK-NOT: @inst : @SUB[
nsl.module @TopArr3 {
  // CHECK: nsl.submodule @inst_0 : @SUB
  // CHECK: nsl.submodule @inst_1 : @SUB
  // CHECK: nsl.submodule @inst_2 : @SUB
  nsl.submodule @inst : @SUB[3]
}

// CHECK-LABEL: nsl.module @SUB
nsl.module @SUB {
}
