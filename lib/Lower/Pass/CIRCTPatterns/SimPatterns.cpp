// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// lib/Lower/Pass/CIRCTPatterns/SimPatterns.cpp — M6 simulation-task
// patterns (Phase 2 scaffold; bodies land at T119–T125 in Phase 6
// US4).
//
// **Design §10 rows covered**: nsl.{sim_display, sim_finish, sim_init,
// sim_delay} + the S29 _init block. Per spec Q1 specify-time → B,
// _init lowers to sv.initial under sv.ifdef "SIMULATION". Per
// research.md §9, all sim-only ops in a single hw.module share ONE
// sv.ifdef block (lazy materialise on first sim op encountered).

#include "../NSLToCIRCTPass.h"

namespace nsl::lower {

void populateSimPatterns(mlir::RewritePatternSet & /*patterns*/,
                         CIRCTTypeConverter & /*type_converter*/) {
  // Phase 2 scaffold — no patterns registered yet.
}

} // namespace nsl::lower
