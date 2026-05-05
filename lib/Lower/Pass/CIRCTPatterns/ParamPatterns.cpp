// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// lib/Lower/Pass/CIRCTPatterns/ParamPatterns.cpp — M6 parameter +
// submodule patterns. At Phase 4 (US2), this family file is
// INTENTIONALLY empty.
//
// Why empty: the structural rewrite in `ModulePatterns.cpp`
// (`lowerNSLModulesToHWModules`) handles ALL of:
//   - `nsl::SubmoduleOp` (singleton form) → `hw::InstanceOp`
//     referencing the paired-and-already-lowered `hw::HWModuleOp`.
//     The instance's parameter array is built from sibling top-level
//     `nsl::ParamIntOp` / `nsl::ParamStrOp` ops via
//     `collectInstanceParameters`.
//   - `nsl::ParamIntOp` / `nsl::ParamStrOp` — consumed by
//     `collectInstanceParameters` then erased by the structural
//     rewrite's straggler sweep.
//
// This consolidation is intentional: per design line 1278, "every
// consuming `hw.instance` carries every top-level param" — the
// resolution of which params to attach to which instance happens
// in the same source-order walk as the SubmoduleOp instantiation,
// so splitting the logic into separate DialectConversion patterns
// would force a redundant lookup pass. The simplification is
// acceptable for the Phase-4 fixture set (T037, T041) which
// exercise a single submodule + a single param. A future M4
// amendment surfacing per-instance param assignments (currently
// missing — see `nsl::SubmoduleOp` carrying only `templateRef` +
// `array_size`, no per-instance param map) will refine this
// per-instance binding.

#include "../NSLToCIRCTPass.h"

namespace nsl::lower {

void populateParamPatterns(mlir::RewritePatternSet & /*patterns*/,
                           CIRCTTypeConverter & /*type_converter*/) {
  // No patterns at Phase 4 — see file header for why.
}

} // namespace nsl::lower
