// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// lib/Lower/Pass/CIRCTPatterns/FSMPatterns.cpp — M6 FSM lowering
// patterns (Phase 2 scaffold; bodies land at T051–T059 in Phase 5
// US3).
//
// **Design §10 rows covered**: nsl.{proc,state,first_state,seq,
// goto (state form),goto (label form),finish,call (proc variant)}
// per data-model.md §3 (8 patterns). The README's named M6 pattern:
// nsl::ProcOp / nsl::StateOp / nsl::SeqOp lower to fsm::MachineOp.

#include "../NSLToCIRCTPass.h"

namespace nsl::lower {

void populateFSMPatterns(mlir::RewritePatternSet & /*patterns*/,
                         CIRCTTypeConverter & /*type_converter*/) {
  // Phase 2 scaffold — no patterns registered yet.
}

} // namespace nsl::lower
