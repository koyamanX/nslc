// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// lib/Lower/Pass/CIRCTPatterns/ArithPatterns.cpp — M6 arithmetic
// conversion patterns (Phase 2 scaffold; pattern bodies land at
// T098–T100 in Phase 6 US4).
//
// **Design §10 rows covered**: nsl.{add,sub,mul,eq,ne,lt,le,gt,ge}
// → comb.{add,sub,mul,icmp}. Per spec Q1 → A: comb-only (no hwarith).
// Per data-model.md §3: 9 patterns total.

#include "../NSLToCIRCTPass.h"

namespace nsl::lower {

void populateArithPatterns(mlir::RewritePatternSet & /*patterns*/,
                           CIRCTTypeConverter & /*type_converter*/) {
  // Phase 2 scaffold — no patterns registered yet. Phase 6 T098–T100
  // populate this body with the 9 arithmetic conversion patterns.
}

} // namespace nsl::lower
