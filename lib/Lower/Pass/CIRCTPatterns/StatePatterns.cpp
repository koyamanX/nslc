// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// lib/Lower/Pass/CIRCTPatterns/StatePatterns.cpp — M6 state-element
// patterns (Phase 2 scaffold; bodies land at T108–T113 in Phase 6
// US4).
//
// **Design §10 rows covered**: nsl.{reg, wire, mem, transfer,
// clocked_transfer} → seq.{firreg|compreg, firmem} + hw.wire +
// direct value substitution. Per spec Q2 → C, default reg lowering
// is seq.firreg with async-active-low rst_n; explicit `interface`
// path uses seq.compreg per firreg-convention.contract.md §1–§2.

#include "../NSLToCIRCTPass.h"

namespace nsl::lower {

void populateStatePatterns(mlir::RewritePatternSet & /*patterns*/,
                           CIRCTTypeConverter & /*type_converter*/) {
  // Phase 2 scaffold — no patterns registered yet.
}

} // namespace nsl::lower
