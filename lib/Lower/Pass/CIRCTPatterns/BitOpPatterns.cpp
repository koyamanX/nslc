// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// lib/Lower/Pass/CIRCTPatterns/BitOpPatterns.cpp — M6 bit-op
// conversion patterns family file.
//
// **Phase 6 status (2026-05-04)**: bit-op lowering bodies live inline
// in `ModulePatterns.cpp`'s `lowerBitOp` helper. Rationale: the same
// Phase 4 / Phase 5 inline-structural-pre-pass strategy applies (see
// `ModulePatterns.cpp` file header).
//
// **Design §10 rows covered (via `lowerBitOp` + `lowerArithOp`)**:
//   * Bitwise `and`/`or`/`xor` (handled by `lowerArithOp`).
//   * Shifts: `nsl.shl` → `comb.shl`, `nsl.shr` → `comb.shru`
//     (logical/unsigned right shift; the dialect documents only
//     unsigned shifts at this layer).
//   * Logical: `nsl.lnot` → `comb.icmp eq %a, 0`; `nsl.not` →
//     `comb.xor %a, all-ones`; `nsl.neg` → `comb.sub 0, %a`.
//   * Reductions: `nsl.reduce_and` → `comb.icmp eq %a, all-ones`;
//     `nsl.reduce_or` → `comb.icmp ne %a, 0`; `nsl.reduce_xor` →
//     `comb.parity %a`.
//   * Width-changing: `nsl.sign_extend` → `comb.concat (replicate
//     MSB, operand)` per Q1 → A; `nsl.zero_extend` → `comb.concat
//     (zeros, operand)`.
//   * Concat / extract / repeat / mux:
//       `nsl.concat` → `comb.concat`
//       `nsl.extract` → `comb.extract`
//       `nsl.repeat` → `comb.replicate`
//       `nsl.mux` (3-input) → `comb.mux`.
//
// **Coverage guard**: empty `populateBitOpPatterns`; per-family
// fixtures live under `test/Lower/circt/arith/` (shared dir per
// `coverage_guard.cmake`).

#include "../NSLToCIRCTPass.h"

namespace nsl::lower {

void populateBitOpPatterns(mlir::RewritePatternSet & /*patterns*/,
                           CIRCTTypeConverter & /*type_converter*/) {
  // M6 bit-op lowering is inline in `ModulePatterns.cpp::lowerBitOp`.
}

} // namespace nsl::lower
