// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// lib/Lower/Pass/CIRCTPatterns/ParamPatterns.cpp — M6 parameter +
// submodule patterns (Phase 2 scaffold; bodies land at T039–T041b,
// T043 in Phase 4 US2).
//
// **Design §10 rows covered**: nsl.{param_int, param_str, submodule
// (singleton form)}. Array-form submodules already exploded by M5's
// NSLExplodeSubmodArrayPass per data-model.md §3.

#include "../NSLToCIRCTPass.h"

namespace nsl::lower {

void populateParamPatterns(mlir::RewritePatternSet & /*patterns*/,
                           CIRCTTypeConverter & /*type_converter*/) {
  // Phase 2 scaffold — no patterns registered yet.
}

} // namespace nsl::lower
