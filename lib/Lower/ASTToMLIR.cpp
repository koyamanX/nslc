// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// lib/Lower/ASTToMLIR.cpp — visitor implementation (M5, layer 8a).
//
// **Specification anchors**:
//   - `specs/008-m5-structural-passes/spec.md` FR-004, FR-005, FR-006.
//   - `specs/008-m5-structural-passes/data-model.md` §1.
//   - `specs/008-m5-structural-passes/research.md` §4.
//
// Phase 3 (US1) is incremental: visit(CompilationUnit) +
// visit(ModuleBlock) emit real `nsl::*` ops; the remaining 52 node
// kinds are no-op stubs satisfying ASTVisitor's pure-virtual
// contract. Real implementations land via tasks.md T047–T056 as
// per-AST-node fixtures (T024–T045) drive each green.

#include "ASTToMLIR.h"

#include "nsl/AST/CompilationUnit.h"
#include "nsl/AST/ModuleBlock.h"
#include "nsl/Dialect/NSL/IR/NSLDialect.h"
#include "nsl/Sema/Sema.h"

#include "mlir/IR/Block.h"
#include "mlir/IR/Builders.h"
#include "mlir/IR/BuiltinOps.h"

namespace nsl::lower {

ASTToMLIR::ASTToMLIR(mlir::MLIRContext &ctx, const sema::SemaResult &sr)
    : ctx_(ctx), sr_(sr), builder_(&ctx) {}

ASTToMLIR::~ASTToMLIR() = default;

mlir::OwningOpRef<mlir::ModuleOp>
ASTToMLIR::lower(const ast::CompilationUnit &cu) {
  auto loc = builder_.getUnknownLoc();
  top_module_ = mlir::ModuleOp::create(builder_, loc);
  builder_.setInsertionPointToStart(top_module_.getBody());
  cu.accept(*this);
  return mlir::OwningOpRef<mlir::ModuleOp>(top_module_);
}

// ---------- Real implementations (US1 P1 increments) ----------

void ASTToMLIR::visit(const ast::CompilationUnit &node) {
  for (const auto &item : node.items()) {
    item->accept(*this);
  }
}

void ASTToMLIR::visit(const ast::ModuleBlock &node) {
  // FR-006 row 1: ast::ModuleBlock → nsl.module @name. Body content
  // (internals/actions/funcs/procs) lands incrementally as US1
  // sub-tasks fill in their respective visit() overrides. At T024
  // (empty module fixture) we emit just the shell.
  auto loc = builder_.getUnknownLoc();
  auto module_op = nsl::dialect::ModuleOp::create(
      builder_, loc, builder_.getStringAttr(node.name()));
  // SizedRegion<1>:$body + SingleBlock + NoTerminator: the body
  // region must contain exactly one block. The auto-builder leaves
  // the region empty; create the entry block here.
  module_op.getBody().emplaceBlock();
}

// ---------- No-op stubs for the remaining 52 AST node kinds ----------
//
// As US1 sub-tasks fill in real visit() bodies, the corresponding
// line is removed from this block (and a real implementation is
// added above).

#define STUB(EnumName)                                                         \
  void ASTToMLIR::visit(const ast::EnumName & /*node*/) {}

STUB(StructDecl)
STUB(TopLevelParamDecl)
STUB(DeclareBlock)
STUB(PortDecl)
STUB(RegDecl)
STUB(WireDecl)
STUB(VariableDecl)
STUB(IntegerDecl)
STUB(MemDecl)
STUB(FuncSelfDecl)
STUB(ProcNameDecl)
STUB(StateNameDecl)
STUB(FirstStateDecl)
STUB(SubmoduleDecl)
STUB(StructInstDecl)
STUB(FuncDefn)
STUB(ProcDefn)
STUB(StateDefn)

STUB(TransferStmt)
STUB(IncDecStmt)
STUB(ControlCallStmt)
STUB(BareFinishStmt)
STUB(SystemTaskStmt)
STUB(ReturnStmt)
STUB(EmptyStmt)
STUB(LabeledStmt)
STUB(GotoStmt)
STUB(InitBlockStmt)
STUB(DelayTaskStmt)
STUB(ParallelBlock)
STUB(AltBlock)
STUB(AnyBlock)
STUB(SeqBlock)
STUB(WhileBlock)
STUB(ForBlock)
STUB(IfStmt)
STUB(StructuralGenerate)

STUB(LiteralExpr)
STUB(IdentifierExpr)
STUB(SystemVarExpr)
STUB(UnaryExpr)
STUB(BinaryExpr)
STUB(ConditionalExpr)
STUB(ConcatExpr)
STUB(RepeatExpr)
STUB(SignExtendExpr)
STUB(ZeroExtendExpr)
STUB(SliceExpr)
STUB(FieldAccessExpr)
STUB(CallExpr)
STUB(StructCastExpr)
STUB(IncDecExpr)

#undef STUB

} // namespace nsl::lower
