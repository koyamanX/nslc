// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// lib/Lower/ASTToMLIR.cpp — visitor implementation skeleton (M5
// Phase 2; per-AST-node visit() bodies land in Phase 3 US1 per
// `tasks.md` T047–T056).
//
// **Specification anchors**:
//   - `specs/008-m5-structural-passes/spec.md` FR-004, FR-005.
//   - `specs/008-m5-structural-passes/data-model.md` §1.
//   - `specs/008-m5-structural-passes/research.md` §4 (single-pass
//     walk + `SymbolTable` lazy resolution).
//
// At Phase 2 the visitor produces an empty `mlir::ModuleOp{}` so
// `Compilation::lowerToNSL` returns a non-null `OwningOpRef` and
// downstream pipeline machinery (`Compilation::runNSLPasses` +
// MLIR's verifier-each) gets a valid IR shape to walk. Per-AST-node
// visit() implementations land in US1.

#include "ASTToMLIR.h"

#include "nsl/AST/CompilationUnit.h"
#include "nsl/Dialect/NSL/IR/NSLDialect.h"
#include "nsl/Sema/Sema.h"

#include "mlir/IR/Builders.h"
#include "mlir/IR/BuiltinOps.h"

namespace nsl::lower {

ASTToMLIR::ASTToMLIR(mlir::MLIRContext &ctx, const sema::SemaResult &sr)
    : ctx_(ctx), sr_(sr), builder_(&ctx) {}

mlir::OwningOpRef<mlir::ModuleOp>
ASTToMLIR::lower(const ast::CompilationUnit & /*cu*/) {
  // Phase 2 skeleton: produce an empty top-level mlir::ModuleOp.
  // Per-AST-node visit() overrides land in Phase 3 (US1) per
  // tasks.md T047–T056. Each will:
  //   1. Use `builder_.create<nsl::dialect::ModuleOp>(loc, ...)` to
  //      emit one `nsl.module` per `ast::ModuleBlock` from `cu`.
  //   2. Attach `mlir::FileLineColLoc`/`FusedLoc` derived from each
  //      AST `SourceRange` (FR-008).
  //   3. Construct `FlatSymbolRefAttr` references lazily; MLIR's
  //      `SymbolTable` resolves them at op-tree finalization (Q4 →
  //      Option A).
  mlir::OpBuilder b(&ctx_);
  auto loc = b.getUnknownLoc();
  // MLIR upstream deprecated `OpBuilder::create<OpTy>` in favor of
  // `OpTy::create(builder, loc, ...)` — use the new form to avoid
  // -Wdeprecated-declarations warnings.
  auto module = mlir::ModuleOp::create(b, loc);
  return mlir::OwningOpRef<mlir::ModuleOp>(module);
}

} // namespace nsl::lower
