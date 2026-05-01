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
#include "nsl/AST/Expr.h"
#include "nsl/AST/FirstStateDecl.h"
#include "nsl/AST/FuncDefn.h"
#include "nsl/AST/LiteralExpr.h"
#include "nsl/AST/MemDecl.h"
#include "nsl/AST/ModuleBlock.h"
#include "nsl/AST/ParallelBlock.h"
#include "nsl/AST/ProcDefn.h"
#include "nsl/AST/RegDecl.h"
#include "nsl/AST/StateDefn.h"
#include "nsl/AST/Stmt.h"
#include "nsl/AST/WireDecl.h"

#include "llvm/ADT/StringRef.h"
#include "llvm/Support/raw_ostream.h"
#include "nsl/Dialect/NSL/IR/NSLDialect.h"
#include "nsl/Sema/Sema.h"

#include "mlir/IR/Block.h"
#include "mlir/IR/Builders.h"
#include "mlir/IR/BuiltinOps.h"

namespace nsl::lower {

namespace {

/// Resolve an AST width-expression to an integer.
///
/// At Phase 3 (US1) this is a minimal parse-the-decimal-literal
/// helper: handles `LiteralExpr` with `Decimal` kind. Other forms
/// (param refs, compile-time constants, hex/binary, etc.) default
/// to 1; richer width resolution lands when the corresponding
/// expression visitors land.
unsigned resolveWidth(const ast::Expr *width_expr) {
  if (!width_expr) {
    return 1; // omitted width → 1-bit per lang.ebnf default
  }
  // Project rule: -fno-rtti, so use NodeKind-based discrimination
  // (the project's own `isa<>` machinery via the `kKind` static).
  if (width_expr->kind() != ast::LiteralExpr::kKind) {
    return 1; // non-literal width → conservative default
  }
  const auto *lit = static_cast<const ast::LiteralExpr *>(width_expr);
  if (lit->litKind() != ast::LiteralExpr::Lit::Decimal) {
    return 1;
  }
  unsigned value = 0;
  for (char c : lit->spelling()) {
    if (c < '0' || c > '9') {
      return 1; // unparseable → conservative default
    }
    value = value * 10 + static_cast<unsigned>(c - '0');
  }
  return value == 0 ? 1 : value;
}

} // namespace

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
  // emitted by recursing into internals (and, in later increments,
  // actions/funcs/procs).
  auto loc = builder_.getUnknownLoc();
  auto module_op = nsl::dialect::ModuleOp::create(
      builder_, loc, builder_.getStringAttr(node.name()));
  // SizedRegion<1>:$body + SingleBlock + NoTerminator: the body
  // region must contain exactly one block.
  auto &body_block = module_op.getBody().emplaceBlock();

  // Recurse into body items at the new insertion point. Save/restore
  // the outer insertion point so subsequent items at the top level
  // continue to land in top_module_.getBody().
  mlir::OpBuilder::InsertionGuard guard(builder_);
  builder_.setInsertionPointToStart(&body_block);
  for (const auto &decl : node.internals()) {
    decl->accept(*this);
  }
  // Recurse into funcs and procs.
  for (const auto &func : node.funcs()) {
    func->accept(*this);
  }
  for (const auto &proc : node.procs()) {
    proc->accept(*this);
  }
  // Recurse into top-level actions (T030+).
  for (const auto &action : node.actions()) {
    action->accept(*this);
  }
}

/// Lower an action-body Stmt directly into the current insertion-
/// point's block, "flattening" a top-level `ast::ParallelBlock` so
/// its children land as direct siblings of the parent op's body
/// (matching the M4 dialect's `nsl.proc` / `nsl.state` body shape
/// in `test/Dialect/atomic/finish_roundtrip.mlir`). For any other
/// Stmt kind, dispatch normally.
void ASTToMLIR::lowerActionBody(const ast::Stmt *body) {
  if (!body) {
    return;
  }
  if (body->kind() != ast::ParallelBlock::kKind) {
    body->accept(*this);
    return;
  }
  // Flatten: visit decls + items at the current insertion point,
  // skipping the wrapping `nsl.parallel`. Items reaching here are
  // semantically the body's parallel children — they belong directly
  // inside the parent op's region.
  const auto &pb = static_cast<const ast::ParallelBlock &>(*body);
  for (const auto &decl : pb.decls()) {
    decl->accept(*this);
  }
  for (const auto &item : pb.items()) {
    item->accept(*this);
  }
}

void ASTToMLIR::visit(const ast::FuncDefn &node) {
  // FR-006 row "FuncDefn → nsl.func @<name> { ... }". Per Q5 →
  // Option A', the `sym_name` is a literal dotted form when the
  // ScopedName has multiple parts (e.g., `inst.method`).
  auto loc = builder_.getUnknownLoc();
  std::string flat_name;
  llvm::raw_string_ostream os(flat_name);
  for (size_t i = 0; i < node.name().parts.size(); ++i) {
    if (i > 0) {
      os << '.';
    }
    os << node.name().parts[i];
  }
  os.flush();
  auto func_op = nsl::dialect::FuncOp::create(
      builder_, loc, builder_.getStringAttr(flat_name));
  auto &body_block = func_op.getBody().emplaceBlock();
  mlir::OpBuilder::InsertionGuard guard(builder_);
  builder_.setInsertionPointToStart(&body_block);
  lowerActionBody(node.body());
}

void ASTToMLIR::visit(const ast::ProcDefn &node) {
  // FR-006 row "ProcDefn → nsl.proc @p { ... }". Body recursion via
  // `lowerActionBody`: the proc body's outer-most ParallelBlock is
  // flattened so child decls (state_name, first_state, state) land
  // directly inside `nsl.proc`'s region — matching the M4 dialect
  // round-trip fixtures (`test/Dialect/atomic/finish_roundtrip.mlir`).
  auto loc = builder_.getUnknownLoc();
  auto proc_op = nsl::dialect::ProcOp::create(
      builder_, loc, builder_.getStringAttr(node.name()));
  auto &body_block = proc_op.getBody().emplaceBlock();
  mlir::OpBuilder::InsertionGuard guard(builder_);
  builder_.setInsertionPointToStart(&body_block);
  lowerActionBody(node.body());
}

void ASTToMLIR::visit(const ast::StateDefn &node) {
  // FR-006 row "StateDefn → nsl.state @s { ... }". Body recursion
  // via lowerActionBody: the state body's outer-most ParallelBlock
  // is flattened so atomic actions (`nsl.transfer`, `nsl.finish`,
  // ...) land directly inside `nsl.state`'s region. Phase 3 ships
  // the shell; transfers/finishes/system-tasks light up alongside
  // their statement-position visitors (T039+).
  auto loc = builder_.getUnknownLoc();
  auto state_op = nsl::dialect::StateOp::create(
      builder_, loc, builder_.getStringAttr(node.name()));
  auto &body_block = state_op.getBody().emplaceBlock();
  mlir::OpBuilder::InsertionGuard guard(builder_);
  builder_.setInsertionPointToStart(&body_block);
  lowerActionBody(node.body());
}

void ASTToMLIR::visit(const ast::FirstStateDecl &node) {
  // FR-006 row "FirstStateDecl → nsl.first_state @s". Single
  // attribute-only op; no region.
  auto loc = builder_.getUnknownLoc();
  auto target = mlir::FlatSymbolRefAttr::get(
      builder_.getStringAttr(node.target()));
  (void)nsl::dialect::FirstStateOp::create(builder_, loc, target);
}

void ASTToMLIR::visit(const ast::RegDecl &node) {
  // FR-006 row "RegDecl → nsl.reg "n" : !nsl.bits<W> = init".
  // At Phase 3 the init expression is dropped (init goes through
  // the expression-lowering pipeline which lands in T055); width
  // resolves via resolveWidth().
  auto loc = builder_.getUnknownLoc();
  auto bits_ty = nsl::dialect::BitsType::get(&ctx_, resolveWidth(node.width()));
  (void)nsl::dialect::RegOp::create(
      builder_, loc, bits_ty, builder_.getStringAttr(node.name()),
      /*init=*/mlir::IntegerAttr{});
}

void ASTToMLIR::visit(const ast::WireDecl &node) {
  // FR-006 row "WireDecl → nsl.wire "n" : !nsl.bits<W>".
  auto loc = builder_.getUnknownLoc();
  auto bits_ty = nsl::dialect::BitsType::get(&ctx_, resolveWidth(node.width()));
  (void)nsl::dialect::WireOp::create(builder_, loc, bits_ty,
                                     builder_.getStringAttr(node.name()));
}

void ASTToMLIR::visit(const ast::MemDecl &node) {
  // FR-006 row "MemDecl → nsl.mem "n" : !nsl.mem<[D x T]>".
  auto loc = builder_.getUnknownLoc();
  auto element_ty =
      nsl::dialect::BitsType::get(&ctx_, resolveWidth(node.width()));
  auto mem_ty =
      nsl::dialect::MemType::get(&ctx_, resolveWidth(node.depth()), element_ty);
  (void)nsl::dialect::MemOp::create(builder_, loc, mem_ty,
                                    builder_.getStringAttr(node.name()));
}

void ASTToMLIR::visit(const ast::ParallelBlock &node) {
  // FR-006 row "ParallelBlock → nsl.parallel { ... }". `nsl.parallel`
  // carries no `HasParent` constraint, so it can appear at module
  // scope or nested inside any action-region. Phase 3 emits the
  // shell with an empty body region; child action recursion lands
  // alongside the other action-block visitors as Stmt-position
  // ops mature (T037+ TransferStmt etc.).
  auto loc = builder_.getUnknownLoc();
  auto par_op = nsl::dialect::ParallelOp::create(builder_, loc);
  auto &body_block = par_op.getBody().emplaceBlock();
  mlir::OpBuilder::InsertionGuard guard(builder_);
  builder_.setInsertionPointToStart(&body_block);
  // Recurse into internal-decls first, then action items, mirroring
  // the AST's split between `decls()` and `items()`.
  for (const auto &decl : node.decls()) {
    decl->accept(*this);
  }
  for (const auto &item : node.items()) {
    item->accept(*this);
  }
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
STUB(VariableDecl)
STUB(IntegerDecl)
STUB(FuncSelfDecl)
STUB(ProcNameDecl)
STUB(StateNameDecl)
STUB(SubmoduleDecl)
STUB(StructInstDecl)

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
