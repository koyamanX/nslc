// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// lib/Lower/Pass/CIRCTPatterns/ModulePatterns.cpp — M6 module-skeleton
// patterns (Phase 2 scaffold; bodies land at T038, T042 in Phase 4
// US2).
//
// **Design §10 rows covered**: nsl.module → hw.HWModuleOp (port list
// derived from paired nsl.declare per circt-lowering.contract.md §3).
// nsl.declare is consumed during ModuleOp lowering.

#include "../NSLToCIRCTPass.h"

namespace nsl::lower {

void populateModulePatterns(mlir::RewritePatternSet & /*patterns*/,
                            CIRCTTypeConverter & /*type_converter*/) {
  // Phase 2 scaffold — no patterns registered yet.
}

} // namespace nsl::lower
