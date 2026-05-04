// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// lib/Lower/Pass/CIRCTPatterns/BitOpPatterns.cpp — M6 bit-op
// conversion patterns (Phase 2 scaffold; bodies land at T101–T107).
//
// **Design §10 rows covered**: nsl.{and,or,xor,shl,shr,land,lor,not,
// neg,lnot,reduce_and,reduce_or,reduce_xor,sign_extend,zero_extend,
// mux,concat,extract,repeat} → comb.* per data-model.md §3 (19
// patterns).

#include "../NSLToCIRCTPass.h"

namespace nsl::lower {

void populateBitOpPatterns(mlir::RewritePatternSet & /*patterns*/,
                           CIRCTTypeConverter & /*type_converter*/) {
  // Phase 2 scaffold — no patterns registered yet.
}

} // namespace nsl::lower
