// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// lib/Lower/Pass/CIRCTPatterns/PortPatterns.cpp — M6 port-direction
// signature legalisation patterns (Phase 2 scaffold; bodies land in
// Phase 4 US2 alongside ModulePatterns).
//
// Most M6 port handling is consumed during the nsl.module →
// hw.HWModuleOp lowering inside ModulePatterns.cpp; PortPatterns.cpp
// owns any standalone port-direction transformations needed for
// func_in/func_out/control-terminal handling that doesn't fit
// inside the ModuleOp rewrite.

#include "../NSLToCIRCTPass.h"

namespace nsl::lower {

void populatePortPatterns(mlir::RewritePatternSet & /*patterns*/,
                          CIRCTTypeConverter & /*type_converter*/) {
  // Phase 2 scaffold — no patterns registered yet.
}

} // namespace nsl::lower
