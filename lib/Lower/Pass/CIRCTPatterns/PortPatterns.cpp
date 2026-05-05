// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// lib/Lower/Pass/CIRCTPatterns/PortPatterns.cpp — M6 port-direction
// signature legalisation patterns. At Phase 4 (US2), this family file
// is INTENTIONALLY empty.
//
// Why empty: the dual-placement port-info-op design (M4 amendment #9)
// has TWO sets of `nsl.input_port` / `nsl.output_port` / `nsl.inout_port`
// ops per module:
//   1. Inside the paired `nsl.declare`'s body — port-list metadata.
//   2. Inside the `nsl.module`'s body — SSA-Value-bearing references
//      (so transfers like `q = a` have valid operands).
//
// Both sets are consumed by `ModulePatterns.cpp`'s structural rewrite
// (`lowerNSLModulesToHWModules` in `NSLToCIRCTPass.h`):
//   * Declare-body ops are erased transitively when the parent
//     `nsl::DeclareOp` is erased after port-list extraction.
//   * Module-body ops are consumed inline during the body walk —
//     each `nsl.input_port`/`nsl.inout_port` result is replaced
//     with the matching `hw::HWModuleOp` block argument; each
//     `nsl.output_port` result is recorded as an output-wiring
//     sink that the final `hw.output` operands draw from.
//
// Therefore no DialectConversion patterns are needed for the port-info
// op classes at Phase 4. If a future phase adds standalone port-handling
// logic (e.g., for `func_in` / `func_out` valid-signal materialisation
// per research §8), that would land here.

#include "../NSLToCIRCTPass.h"

namespace nsl::lower {

void populatePortPatterns(mlir::RewritePatternSet & /*patterns*/,
                          CIRCTTypeConverter & /*type_converter*/) {
  // No patterns at Phase 4 — see file header for why.
}

} // namespace nsl::lower
