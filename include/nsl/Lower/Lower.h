// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// include/nsl/Lower/Lower.h — single public umbrella header for the
// `nsl-lower` library (M5, layer 8a).
//
// **Specification anchors**:
//   - `specs/008-m5-structural-passes/spec.md` FR-001 — single
//     public umbrella header per Constitution Principle II §3.
//     `nsl-lower` is NOT one of the named exceptions for `nsl-ast`
//     and `nsl-sema`; it gets exactly one public header.
//   - `specs/008-m5-structural-passes/contracts/lower-api.contract.md`
//     §2 — frozen public surface (8 free functions: 1 visitor entry +
//     6 pass constructors + 1 registration helper).
//   - `specs/008-m5-structural-passes/data-model.md` §1, §2 — class
//     shape + pipeline ordering.
//
// Consumers (`nsl-driver` member functions `lowerToNSL` /
// `runNSLPasses`, the `tools/nsl-opt` developer/test binary) include
// THIS header only — never the per-pass internal `lib/Lower/Pass/`
// headers. The 8-symbol freeze surface is mechanically auditable via
// `lower-api.contract.md` §6.

#ifndef NSL_LOWER_LOWER_H
#define NSL_LOWER_LOWER_H

#include "mlir/IR/BuiltinOps.h"
#include "mlir/IR/MLIRContext.h"
#include "mlir/IR/OwningOpRef.h"
#include "mlir/Pass/Pass.h"

#include <memory>

namespace nsl::ast {
class CompilationUnit;
} // namespace nsl::ast

namespace nsl::sema {
struct SemaResult;
} // namespace nsl::sema

namespace nsl::lower {

// -------------------------------------------------------------------
// Visitor entry point (FR-005, lower-api.contract.md §2.1)
// -------------------------------------------------------------------

/// Lower a typed AST `CompilationUnit` plus its paired `SemaResult`
/// into a fresh `mlir::ModuleOp` whose body contains one `nsl.module`
/// per `ast::ModuleBlock`. The walk is **single-pass** with MLIR's
/// stock `SymbolTable` lazy resolution per Clarifications Q4 →
/// Option A.
///
/// **Pre-condition**: `sr.hasErrors() == false`. Caller (typically
/// `Compilation::lowerToNSL`) is responsible for the gate.
///
/// **Post-condition**: every emitted `mlir::Operation*` carries a
/// non-`UnknownLoc` `mlir::Location` resolvable to the originating
/// AST `SourceRange` (FR-008).
///
/// **Failure**: returns a failed `OwningOpRef` (test via `bool(...)`)
/// after posting at least one diagnostic to the active
/// `mlir::DiagnosticEngine`. The caller's `DiagnosticBridge` then
/// forwards to `basic::DiagnosticEngine`.
mlir::OwningOpRef<mlir::ModuleOp> astToMLIR(mlir::MLIRContext &ctx,
                                            const ast::CompilationUnit &cu,
                                            const sema::SemaResult &sr);

// -------------------------------------------------------------------
// Pass constructors (FR-011, lower-api.contract.md §2.2)
// -------------------------------------------------------------------
//
// Each pass is `mlir::OperationPass<nsl::dialect::ModuleOp>` and
// registers under a stable command-line flag for `nsl-opt` standalone
// invocation. Pipeline order is FROZEN per FR-012 +
// `pass-pipeline.contract.md` §1.

/// Slot 1 — substitute every `nsl.param_int` / `nsl.param_str`
/// operand reference with the constant value from the M3 Sema
/// parameter map (FR-013).
std::unique_ptr<mlir::Pass> createNSLResolveParamsPass();

/// Slot 2 — replace each `nsl.structural_generate` with N inline
/// copies of its body; substitute the loop-variable `%IDENT%`
/// references with per-iteration constants (FR-014).
std::unique_ptr<mlir::Pass> createNSLExpandGeneratePass();

/// Slot 3 — replace each `nsl.variable` op with an SSA chain of
/// `nsl.wire` + `nsl.transfer` ops (struct-SSA-split). Per-field
/// decomposition for struct-typed variables (FR-015).
std::unique_ptr<mlir::Pass> createNSLExpandVariablesPass();

/// Slot 4 — replace array-form `nsl.submodule` (`SUB[3]`) with N
/// independent ops named `SUB[0]` … `SUB[N-1]`; rewrite cross-IR
/// port references (FR-016).
std::unique_ptr<mlir::Pass> createNSLExplodeSubmodArrayPass();

/// Slot 5 — registered no-op slot at M5 per Clarifications Q3 →
/// Option B. Reserves the pipeline ABI for a future PR to fill in
/// functional `func_self` inlining (FR-017).
std::unique_ptr<mlir::Pass> createNSLInlineInternalFuncPass();

/// Slot 6 — final correctness gate. Detects post-expansion `%IDENT%`
/// residue via regex over `mlir::StringAttr` values; re-checks the
/// six post-expansion-sensitive `Sn` constraints (S6/S10/S15/S16/S20/S25
/// per `pass-pipeline.contract.md` §3) (FR-018).
std::unique_ptr<mlir::Pass> createNSLCheckSemanticsPass();

// -------------------------------------------------------------------
// Pass registration (lower-api.contract.md §2.3)
// -------------------------------------------------------------------

/// Register all six M5 passes with MLIR's pass-registry so they are
/// discoverable by name from `nsl-opt -<flag>`. Idempotent: the
/// underlying `mlir::registerPass` is idempotent by design.
///
/// Called from `tools/nsl-opt/main.cpp` after
/// `nsl::dialect::registerNSLDialect()`.
void registerNSLLowerPasses();

} // namespace nsl::lower

#endif // NSL_LOWER_LOWER_H
