// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// lib/Lower/Pass/CIRCTPatterns/ControlPatterns.cpp — M6 control-flow
// patterns family file.
//
// **Phase 6 status (2026-05-04)**: control-flow lowering bodies live
// inline in `ModulePatterns.cpp`'s `lowerIfOp` / `lowerAltOp` /
// `lowerAnyOp` / `lowerCallOp` helpers (orchestrated by the
// `lowerControlOp` dispatch helper). Rationale: the recursive
// region walk + condition-gate threading is incompatible with
// DialectConversion's per-op rewrite worklist (see
// `ModulePatterns.cpp` file header for the full architectural
// reasoning).
//
// **Design §10 rows covered (via the inline helpers)**:
//   * `nsl.if` (statement, wire LHS) → `comb::MuxOp`.
//   * `nsl.if` (statement, reg LHS) → mux-on-data via the
//     `RegInfo.pendingNext` chain per Q3 → A and
//     `firreg-convention.contract.md` §3.
//   * `nsl.alt` → priority chain via cumulative `coveredSoFar`
//     OR-accumulator + per-case `gated = (!coveredSoFar) AND
//     caseCond` mux conditions (S13 priority semantics; equivalent
//     to circt-lowering.contract.md §4's right-associative nested
//     `comb.mux` form).
//   * `nsl.any` → per-case independent `gated = parentGate AND
//     caseCond` (S13 parallel; equivalent to §5's per-target
//     `comb.or` of `comb.mux(cond, val, 0)` envelopes).
//   * `nsl.call` (func_in target) → `<func>_valid` `hw::WireOp`
//     materialisation; the func body is inlined when its FuncOp is
//     encountered. Disambiguation against proc-target call (Phase 5
//     FSM pre-pass): any `nsl.call` left at Phase 6 is presumed
//     func_in-target, since Phase 5 already consumed all proc-target
//     calls.
//
// **Coverage guard**: empty `populateControlPatterns`; per-family
// fixtures live under `test/Lower/circt/control/`.

#include "../NSLToCIRCTPass.h"

namespace nsl::lower {

void populateControlPatterns(mlir::RewritePatternSet & /*patterns*/,
                             CIRCTTypeConverter & /*type_converter*/) {
  // M6 control-flow lowering is inline in `ModulePatterns.cpp`.
}

} // namespace nsl::lower
