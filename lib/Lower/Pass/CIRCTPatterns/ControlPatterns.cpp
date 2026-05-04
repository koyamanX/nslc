// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// lib/Lower/Pass/CIRCTPatterns/ControlPatterns.cpp — M6 control-flow
// conversion patterns (Phase 2 scaffold; bodies land at T114–T118).
//
// **Design §10 rows covered**: nsl.{alt,any,if,call (func_in
// variant)} per data-model.md §3 (4 patterns). Per spec Q3 → A,
// `nsl.if` over reg LHS uses the mux-on-data strategy (data-input
// `comb.mux`); see firreg-convention.contract.md §3.

#include "../NSLToCIRCTPass.h"

namespace nsl::lower {

void populateControlPatterns(mlir::RewritePatternSet & /*patterns*/,
                             CIRCTTypeConverter & /*type_converter*/) {
  // Phase 2 scaffold — no patterns registered yet.
}

} // namespace nsl::lower
