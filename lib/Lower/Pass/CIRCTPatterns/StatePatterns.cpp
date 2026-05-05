// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// lib/Lower/Pass/CIRCTPatterns/StatePatterns.cpp — M6 state-element
// patterns family file.
//
// **Phase 6 status (2026-05-04)**: state-element lowering bodies live
// inline in `ModulePatterns.cpp`'s `lowerRegOp` / `lowerWireOp` /
// `lowerMemOp` / `lowerTransferOp` / `lowerClockedTransferOp` helpers.
// Rationale: the inline-structural-pre-pass strategy from Phase 4 /
// Phase 5 also applies here — the reg-update path requires threading
// a `RegInfo.pendingNext` mux chain through if/alt/any conditional
// scopes, which is incompatible with DialectConversion's per-op
// rewrite worklist.
//
// **Design §10 rows covered (via the inline helpers)**:
//   * `nsl.reg` (no `interface`) → `seq::FirRegOp` with async-active-
//     low reset wired through `comb::ICmpOp eq %rst_n, 0` per
//     `firreg-convention.contract.md` §1 (Q2 → C).
//   * `nsl.reg` (with `interface`) → `seq::CompRegOp` with user-named
//     clock/reset operands per §2.
//   * `nsl.wire` → `hw::WireOp` (lazy materialisation on first
//     driving transfer — preserves SSA dominance).
//   * `nsl.mem` → `seq::FirMemOp` with depth + width preserved.
//   * `nsl.transfer` (combinational `=`) → output-port wiring or
//     wire-driver; no standalone CIRCT op.
//   * `nsl.clocked_transfer` (`:=`) → `seq::FirRegOp` data-input
//     write (mux-on-data per Q3 → A; conditional updates flow through
//     the `RegInfo.pendingNext` chain).
//
// **Reset polarity convention**: §1 — async + active-low; the FirRegOp
// `reset` operand is connected to a `comb::ICmpOp eq %rst_n, 0`
// derived condition (i.e., reset fires when `rst_n` is low). Reset
// value defaults to the `nsl.reg` initialiser literal (or 0 when
// absent per S2/S23). All firregs in a no-`interface` module share
// the implicit `clk` + `rst_n` ports.
//
// **Coverage guard**: empty `populateStatePatterns`; per-family
// fixtures live under `test/Lower/circt/state/`.

#include "../NSLToCIRCTPass.h"

namespace nsl::lower {

void populateStatePatterns(mlir::RewritePatternSet & /*patterns*/,
                           CIRCTTypeConverter & /*type_converter*/) {
  // M6 state-element lowering is inline in `ModulePatterns.cpp`.
}

} // namespace nsl::lower
