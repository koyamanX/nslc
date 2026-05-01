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

#include "nsl/AST/BareFinishStmt.h"
#include "nsl/AST/CompilationUnit.h"
#include "nsl/AST/DelayTaskStmt.h"
#include "nsl/AST/Expr.h"
#include "nsl/AST/FirstStateDecl.h"
#include "nsl/AST/FuncDefn.h"
#include "nsl/AST/InitBlockStmt.h"
#include "nsl/AST/LiteralExpr.h"
#include "nsl/AST/MemDecl.h"
#include "nsl/AST/ModuleBlock.h"
#include "nsl/AST/ParallelBlock.h"
#include "nsl/AST/ProcDefn.h"
#include "nsl/AST/RegDecl.h"
#include "nsl/AST/SeqBlock.h"
#include "nsl/AST/StateDefn.h"
#include "nsl/AST/Stmt.h"
#include "nsl/AST/SystemTaskStmt.h"
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
/// Strip the surrounding double quotes from a `LiteralExpr::String`
/// spelling. The lexer preserves the verbatim source text (per
/// `LiteralExpr.h` line 35), so a `"done"` source token shows up
/// here as the 6-character string `"done"` — caller wants the
/// 4-character interior. Escape-sequence rewriting (e.g., `\n`) is
/// out of scope at Phase 3; the M3 corpus exercises only literal
/// content.
llvm::StringRef unquoteStringLiteral(llvm::StringRef spelling) {
  if (spelling.size() >= 2 && spelling.front() == '"' &&
      spelling.back() == '"') {
    return spelling.substr(1, spelling.size() - 2);
  }
  return spelling;
}

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

/// Resolve a decimal-literal `Expr` to an `int64_t`. Phase 3 helper
/// used for op-attribute integer payloads (e.g., `nsl.sim_delay
/// $cycles`); richer int evaluation (param refs, hex/bin, sized
/// literals) lands with the expression sub-visitor (T055). Returns
/// 0 if the expression is not a parseable decimal literal.
int64_t resolveDecimalLiteral(const ast::Expr *expr) {
  if (!expr || expr->kind() != ast::LiteralExpr::kKind) {
    return 0;
  }
  const auto *lit = static_cast<const ast::LiteralExpr *>(expr);
  if (lit->litKind() != ast::LiteralExpr::Lit::Decimal) {
    return 0;
  }
  int64_t value = 0;
  for (char c : lit->spelling()) {
    if (c < '0' || c > '9') {
      return 0;
    }
    value = value * 10 + static_cast<int64_t>(c - '0');
  }
  return value;
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
  // The init expression is lowered to an `OptionalAttr<I64Attr>` via
  // `resolveDecimalLiteral` — Phase 3 only handles a Decimal literal
  // initialiser (richer init expressions, e.g., param refs / hex /
  // binary, land alongside the expression sub-visitor in T055).
  // Absence of an init clause leaves the attribute null (printer
  // omits the `= …` form).
  auto loc = builder_.getUnknownLoc();
  auto bits_ty = nsl::dialect::BitsType::get(&ctx_, resolveWidth(node.width()));
  mlir::IntegerAttr init_attr;
  if (const ast::Expr *init = node.init();
      init && init->kind() == ast::LiteralExpr::kKind &&
      static_cast<const ast::LiteralExpr *>(init)->litKind() ==
          ast::LiteralExpr::Lit::Decimal) {
    init_attr = builder_.getI64IntegerAttr(resolveDecimalLiteral(init));
  }
  (void)nsl::dialect::RegOp::create(
      builder_, loc, bits_ty, builder_.getStringAttr(node.name()), init_attr);
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

void ASTToMLIR::visit(const ast::InitBlockStmt &node) {
  // FR-006 row "SystemTaskStmt × {..., _init, ...} → ... /
  // nsl.sim_init / ...". `_init { ... }` reaches the AST as the
  // dedicated `InitBlockStmt` (per ParseStmt.cpp:1156-1157).
  // `nsl.sim_init` requires `HasParent<"ModuleOp">` (S29 enforced
  // upstream by Sema). Body items recurse into the new region.
  auto loc = builder_.getUnknownLoc();
  auto init_op = nsl::dialect::SimInitOp::create(builder_, loc);
  auto &body_block = init_op.getBody().emplaceBlock();
  mlir::OpBuilder::InsertionGuard guard(builder_);
  builder_.setInsertionPointToStart(&body_block);
  for (const auto &item : node.items()) {
    item->accept(*this);
  }
}

void ASTToMLIR::visit(const ast::DelayTaskStmt &node) {
  // FR-006 row "SystemTaskStmt × {..., _delay} → ... / nsl.sim_delay".
  // `_delay(N)` reaches the AST as the dedicated `DelayTaskStmt`
  // (per ParseStmt.cpp:1159-1160). `nsl.sim_delay` carries an
  // `I64Attr:$cycles` and accepts `ParentOneOf<["ModuleOp",
  // "SimInitOp"]>`. Phase 3 reads the count from a decimal literal;
  // richer count expressions land with T055.
  auto loc = builder_.getUnknownLoc();
  int64_t cycles = resolveDecimalLiteral(node.count());
  (void)nsl::dialect::SimDelayOp::create(builder_, loc,
                                         builder_.getI64IntegerAttr(cycles));
}

void ASTToMLIR::visit(const ast::SystemTaskStmt &node) {
  // FR-006 row "SystemTaskStmt × {_display, _finish, _init, _delay}
  // → nsl.sim_display / nsl.sim_finish / nsl.sim_init / nsl.sim_delay".
  // Note: `_init` and `_delay` reach the AST as separate node kinds
  // (`InitBlockStmt`, `DelayTaskStmt`) per `ParseStmt.cpp:1156-1161`,
  // so this visitor handles only `_display` and `_finish` (plus
  // anything else that lands as a `SystemTaskStmt`).
  //
  // At Phase 3 (US1) — variadic operand expressions are NOT yet
  // lowered (T055 ships the expression sub-visitor). The two
  // forms we handle here both consume a single literal-string
  // first argument (the format string for `_display`, the reason
  // for `_finish`) and emit no operands.
  auto loc = builder_.getUnknownLoc();
  llvm::StringRef name = node.name();
  llvm::StringRef literal_arg;
  if (!node.args().empty() &&
      node.args().front()->kind() == ast::LiteralExpr::kKind) {
    const auto *lit =
        static_cast<const ast::LiteralExpr *>(node.args().front().get());
    if (lit->litKind() == ast::LiteralExpr::Lit::String) {
      literal_arg = unquoteStringLiteral(lit->spelling());
    }
  }
  if (name == "_finish") {
    (void)nsl::dialect::SimFinishOp::create(
        builder_, loc, builder_.getStringAttr(literal_arg));
    return;
  }
  if (name == "_display") {
    // `nsl.sim_display` carries `format:StrAttr` plus zero-or-more
    // variadic operand args. At Phase 3, only the format string is
    // lowered (variadic operands need expression-Value plumbing
    // landing in T055).
    (void)nsl::dialect::SimDisplayOp::create(
        builder_, loc, builder_.getStringAttr(literal_arg),
        /*args=*/mlir::ValueRange{});
    return;
  }
  // Other SystemTaskStmt names (e.g., `_readmemh`) reach here at
  // Phase 3 with no MLIR counterpart yet. Drop on the floor — Phase
  // 3 only exercises the `_finish` / `_display` flavours; richer
  // task lowering lands in a follow-up.
}

void ASTToMLIR::visit(const ast::BareFinishStmt & /*node*/) {
  // FR-006 row "BareFinishStmt → nsl.finish". `nsl.finish` carries
  // no operands and has a transitive-parent verifier (per Q2
  // Option B) requiring an ancestor `nsl.proc` — the input AST is
  // Sema-clean per FR-010 so that ancestor invariant holds by
  // construction at every reachable insertion point.
  auto loc = builder_.getUnknownLoc();
  (void)nsl::dialect::FinishOp::create(builder_, loc);
}

void ASTToMLIR::visit(const ast::SeqBlock &node) {
  // FR-006 row "SeqBlock → nsl.seq { ... }". `nsl.seq` carries
  // `HasParent<"FuncOp">` (per NSLOps.td §2.4); the input AST is
  // Sema-clean per FR-010 so the parent invariant holds at every
  // reachable insertion point. Body items recursion mirrors
  // ParallelBlock; expression-Value plumbing for items lands in
  // T055.
  auto loc = builder_.getUnknownLoc();
  auto seq_op = nsl::dialect::SeqOp::create(builder_, loc);
  auto &body_block = seq_op.getBody().emplaceBlock();
  mlir::OpBuilder::InsertionGuard guard(builder_);
  builder_.setInsertionPointToStart(&body_block);
  for (const auto &decl : node.decls()) {
    decl->accept(*this);
  }
  for (const auto &item : node.items()) {
    item->accept(*this);
  }
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
STUB(ReturnStmt)
STUB(EmptyStmt)
STUB(LabeledStmt)
STUB(GotoStmt)
STUB(AltBlock)
STUB(AnyBlock)
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
