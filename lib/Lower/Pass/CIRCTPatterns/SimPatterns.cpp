// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// lib/Lower/Pass/CIRCTPatterns/SimPatterns.cpp — M6 simulation-task
// patterns family file.
//
// **Phase 6 status (2026-05-04)**: sim-task lowering bodies live
// inline in `ModulePatterns.cpp`'s `lowerSimDisplayOp` /
// `lowerSimFinishOp` / `lowerSimDelayOp` / `lowerSimInitOp` /
// `lowerTopLevelSimOp` helpers, plus the per-module ifdef helper
// `getOrBuildSimIfDef` and the macro-decl helper
// `ensureSimulationMacroDecl`. Rationale: the lazy-materialise
// per-module ifdef pattern requires shared mutable state across
// multiple sim ops within a single hw.module, which is
// incompatible with DialectConversion's stateless per-op rewrite
// worklist.
//
// **Design §10 rows covered (via the inline helpers)**:
//   * `nsl.sim_display` → `sv::FWriteOp` to fd=1 (stdout) inside
//     the per-module SIMULATION ifdef.
//   * `nsl.sim_finish` → `sv::FinishOp` with default behavior
//     code 1.
//   * `nsl.sim_init` → wraps body's child sim ops in
//     `sv::InitialOp` inside the per-module SIMULATION ifdef.
//   * `nsl.sim_delay` → `sv::VerbatimOp "#N;"` (CIRCT has no
//     dedicated `sv::DelayOp`; verbatim is the idiomatic emission
//     for inline `#delay;` per SV LRM 14.10).
//   * S29 `_init { ... }` block → covered by SimInitOp (M5 emits
//     `nsl.sim_init` for the S29 block syntax).
//
// **Per-module ifdef sharing (research §9)**: at most ONE
// `sv::IfDefOp @SIMULATION` per `hw::HWModuleOp` body; all sim ops
// in that module share it. The ifdef sits at the END of the
// hw.module body (after synthesizable ops). A single
// `sv::MacroDeclOp @SIMULATION` is materialised at the outer
// `mlir::ModuleOp` level (idempotent across multiple hw.modules
// per `ensureSimulationMacroDecl`), required by `sv::IfDefOp`'s
// `MacroIdentAttr` SymbolUserOpInterface.
//
// **Coverage guard**: empty `populateSimPatterns`; per-family
// fixtures live under `test/Lower/circt/sim/`.

#include "../NSLToCIRCTPass.h"

namespace nsl::lower {

void populateSimPatterns(mlir::RewritePatternSet & /*patterns*/,
                         CIRCTTypeConverter & /*type_converter*/) {
  // M6 sim-task lowering is inline in `ModulePatterns.cpp`.
}

} // namespace nsl::lower
