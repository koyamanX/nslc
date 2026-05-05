// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// lib/Lower/Pass/CIRCTPatterns/ArithPatterns.cpp — M6 arithmetic
// conversion patterns family file.
//
// **Phase 6 status (2026-05-04)**: the arithmetic lowering bodies live
// inline in `lib/Lower/Pass/CIRCTPatterns/ModulePatterns.cpp`'s
// `lowerArithOp` helper, called from the unified
// `lowerNSLModulesToHWModules` structural pre-pass per the Phase 4 /
// Phase 5 precedent (no DialectConversion patterns; the prepass walks
// each `nsl::ModuleOp` body in source order and materialises CIRCT
// ops directly). Rationale: the standard DialectConversion worklist
// would interleave incorrectly with the recursive nsl-region lowering
// (alt/any/if/case/default bodies hold transfer/arith ops that need
// outer-anchor insertion of CIRCT replacements). See
// `ModulePatterns.cpp`'s file-header comment for the full
// architectural reasoning.
//
// **Design §10 rows covered (via `lowerArithOp`)**: nsl.{add,sub,mul,
// eq,ne,lt,le,gt,ge,land,lor} → comb.{add,sub,mul,icmp <pred>,and,or}.
// All comparisons use unsigned predicates per the M4 dialect's
// value-neutral semantics (NSLOps.td §2.2quattuor comment lines
// 626–630). Per spec Q1 → A: comb-only (no `hwarith`).
//
// **Coverage guard**: this file is intentionally devoid of
// `OpConversionPattern<` tokens — the per-family fixture set under
// `test/Lower/circt/arith/` covers the design §10 rows mechanically;
// the `coverage_guard.cmake` bijection check trivially holds because
// the empty `populateArithPatterns` body emits zero patterns and a
// directory with at least one `*.nsl` fixture is present.

#include "../NSLToCIRCTPass.h"

namespace nsl::lower {

void populateArithPatterns(mlir::RewritePatternSet & /*patterns*/,
                           CIRCTTypeConverter & /*type_converter*/) {
  // M6 arithmetic lowering is performed inline in
  // `ModulePatterns.cpp::lowerArithOp` per the Phase 4 / Phase 5
  // precedent. See file-header comment.
}

} // namespace nsl::lower
