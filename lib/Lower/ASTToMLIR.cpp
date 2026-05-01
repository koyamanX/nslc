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

#include <algorithm>

#include "nsl/AST/AltBlock.h"
#include "nsl/AST/AnyBlock.h"
#include "nsl/AST/BareFinishStmt.h"
#include "nsl/AST/BinaryExpr.h"
#include "nsl/AST/CompilationUnit.h"
#include "nsl/AST/ConcatExpr.h"
#include "nsl/AST/ConditionalExpr.h"
#include "nsl/AST/ControlCallStmt.h"
#include "nsl/AST/DelayTaskStmt.h"
#include "nsl/AST/Expr.h"
#include "nsl/AST/FieldAccessExpr.h"
#include "nsl/AST/FirstStateDecl.h"
#include "nsl/AST/ForBlock.h"
#include "nsl/AST/FuncDefn.h"
#include "nsl/AST/IdentifierExpr.h"
#include "nsl/AST/IfStmt.h"
#include "nsl/AST/IncDecStmt.h"
#include "nsl/AST/InitBlockStmt.h"
#include "nsl/AST/LiteralExpr.h"
#include "nsl/AST/MemDecl.h"
#include "nsl/AST/ModuleBlock.h"
#include "nsl/AST/ParallelBlock.h"
#include "nsl/AST/ProcDefn.h"
#include "nsl/AST/RegDecl.h"
#include "nsl/AST/SeqBlock.h"
#include "nsl/AST/SignExtendExpr.h"
#include "nsl/AST/SliceExpr.h"
#include "nsl/AST/StateDefn.h"
#include "nsl/AST/Stmt.h"
#include "nsl/AST/StructCastExpr.h"
#include "nsl/AST/StructDecl.h"
#include "nsl/AST/StructInstDecl.h"
#include "nsl/AST/SystemTaskStmt.h"
#include "nsl/AST/TransferStmt.h"
#include "nsl/AST/UnaryExpr.h"
#include "nsl/AST/WhileBlock.h"
#include "nsl/AST/WireDecl.h"
#include "nsl/AST/ZeroExtendExpr.h"

#include "llvm/ADT/StringRef.h"
#include "llvm/Support/raw_ostream.h"
#include "nsl/Dialect/NSL/IR/NSLDialect.h"
#include "nsl/Sema/Sema.h"

#include "mlir/IR/Block.h"
#include "mlir/IR/Builders.h"
#include "mlir/IR/BuiltinAttributes.h"
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
  auto reg_op = nsl::dialect::RegOp::create(
      builder_, loc, bits_ty, builder_.getStringAttr(node.name()), init_attr);
  // Register the SSA result under the AST identifier so transfer-
  // statement RHS / LHS identifier resolution can locate this storage
  // (transitional name-table — see header comment).
  nameTable_[node.name()] = reg_op.getResult();
}

void ASTToMLIR::visit(const ast::WireDecl &node) {
  // FR-006 row "WireDecl → nsl.wire "n" : !nsl.bits<W>".
  auto loc = builder_.getUnknownLoc();
  auto bits_ty = nsl::dialect::BitsType::get(&ctx_, resolveWidth(node.width()));
  auto wire_op = nsl::dialect::WireOp::create(
      builder_, loc, bits_ty, builder_.getStringAttr(node.name()));
  nameTable_[node.name()] = wire_op.getResult();
}

void ASTToMLIR::visit(const ast::MemDecl &node) {
  // FR-006 row "MemDecl → nsl.mem "n" : !nsl.mem<[D x T]>".
  auto loc = builder_.getUnknownLoc();
  auto element_ty =
      nsl::dialect::BitsType::get(&ctx_, resolveWidth(node.width()));
  auto mem_ty =
      nsl::dialect::MemType::get(&ctx_, resolveWidth(node.depth()), element_ty);
  auto mem_op = nsl::dialect::MemOp::create(builder_, loc, mem_ty,
                                            builder_.getStringAttr(node.name()));
  nameTable_[node.name()] = mem_op.getResult();
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

// ---------- Expression sub-visitor (lowerExpr) ----------
//
// Phase 3 minimum-viable surface to unblock `nsl.transfer` lowering:
// `LiteralExpr` (Decimal, default 1-bit width per FR-007) plus
// `IdentifierExpr` (lookup in the transitional `nameTable_`). Richer
// expression coverage (Binary / Unary / Conditional / Slice / Concat
// / FieldAccess / SignExtend / ZeroExtend / Repeat / Call / IncDec /
// StructCast / SystemVar) lands incrementally in T055.

mlir::Value ASTToMLIR::lowerExpr(const ast::Expr *expr) {
  if (!expr) {
    return {};
  }
  if (expr->kind() == ast::LiteralExpr::kKind) {
    const auto *lit = static_cast<const ast::LiteralExpr *>(expr);
    if (lit->litKind() != ast::LiteralExpr::Lit::Decimal) {
      // Hex / Binary / Octal / String literals reach here at Phase 3
      // with no expression-position lowering yet (T055 follow-up).
      return {};
    }
    auto loc = builder_.getUnknownLoc();
    // Phase 3 width policy: literals at the expression-tree surface
    // default to 1-bit. Width promotion / inference is the
    // expression-sub-visitor's job in T055; for now downstream
    // SameTypeOperands ops will fail-loudly if the LHS width
    // disagrees, which is the expected behaviour at this surface.
    auto bits_ty = nsl::dialect::BitsType::get(&ctx_, /*width=*/1);
    auto attr = builder_.getI64IntegerAttr(resolveDecimalLiteral(expr));
    auto const_op =
        nsl::dialect::ConstantOp::create(builder_, loc, bits_ty, attr);
    return const_op.getResult();
  }
  if (expr->kind() == ast::IdentifierExpr::kKind) {
    const auto *ident = static_cast<const ast::IdentifierExpr *>(expr);
    // Multi-part dotted names (`inst.field`) reach Sema at M3; at
    // Phase 3 we resolve only single-part identifiers via the local
    // `nameTable_` (Phase 5 / M3-Sema landing flips this to the
    // canonical `Symbol*` lookup path per Q4 → Option A).
    if (ident->name().parts.size() != 1) {
      return {};
    }
    auto it = nameTable_.find(ident->name().parts.front());
    if (it == nameTable_.end()) {
      // Unresolved identifier — Sema-clean inputs (FR-010) would have
      // already flagged this; soft-fail per the offload decision.
      return {};
    }
    return it->getValue();
  }
  if (expr->kind() == ast::BinaryExpr::kKind) {
    // FR-007 row "BinaryExpr → nsl.<op>" per the M4 expression-op
    // surface frozen at 77 (post-amendment cluster 1+2+3+4). LHS/RHS
    // recurse through `lowerExpr`. Six trait-shapes drive the create
    // signature:
    //   - arith / bitwise / shift: `Pure + SameOperandsAndResultType`
    //     create(loc, lhs, rhs) — result type inferred from operands.
    //   - comparison + logical (`==`/`!=`/`<`/`<=`/`>`/`>=`/`&&`/`||`):
    //     `Pure + SameTypeOperands`, result is `!nsl.bits<1>`. The
    //     create overload is create(loc, resultType, lhs, rhs).
    // `Div` and `Mod` from the AST enum have no M4 counterpart (the
    // M4 surface omits them); soft-fail per FR-010 — Sema-clean
    // inputs upstream of M5 wouldn't surface those at this layer.
    const auto *bin = static_cast<const ast::BinaryExpr *>(expr);
    auto lhs_val = lowerExpr(bin->lhs());
    auto rhs_val = lowerExpr(bin->rhs());
    if (!lhs_val || !rhs_val) {
      return {};
    }
    auto loc = builder_.getUnknownLoc();
    auto bits1_ty = nsl::dialect::BitsType::get(&ctx_, /*width=*/1);
    using BO = ast::BinaryExpr::Op;
    switch (bin->op()) {
    // SameOperandsAndResultType: result type inferred from operands.
    case BO::Add:
      return nsl::dialect::AddOp::create(builder_, loc, lhs_val, rhs_val)
          .getResult();
    case BO::Sub:
      return nsl::dialect::SubOp::create(builder_, loc, lhs_val, rhs_val)
          .getResult();
    case BO::Mul:
      return nsl::dialect::MulOp::create(builder_, loc, lhs_val, rhs_val)
          .getResult();
    case BO::BitAnd:
      return nsl::dialect::AndOp::create(builder_, loc, lhs_val, rhs_val)
          .getResult();
    case BO::BitOr:
      return nsl::dialect::OrOp::create(builder_, loc, lhs_val, rhs_val)
          .getResult();
    case BO::BitXor:
      return nsl::dialect::XorOp::create(builder_, loc, lhs_val, rhs_val)
          .getResult();
    case BO::ShiftLeft:
      return nsl::dialect::ShlOp::create(builder_, loc, lhs_val, rhs_val)
          .getResult();
    case BO::ShiftRight:
      return nsl::dialect::ShrOp::create(builder_, loc, lhs_val, rhs_val)
          .getResult();
    // SameTypeOperands; result is !nsl.bits<1>.
    case BO::Equal:
      return nsl::dialect::EqOp::create(builder_, loc, bits1_ty, lhs_val,
                                         rhs_val)
          .getResult();
    case BO::NotEqual:
      return nsl::dialect::NeOp::create(builder_, loc, bits1_ty, lhs_val,
                                         rhs_val)
          .getResult();
    case BO::Less:
      return nsl::dialect::LtOp::create(builder_, loc, bits1_ty, lhs_val,
                                         rhs_val)
          .getResult();
    case BO::LessEqual:
      return nsl::dialect::LeOp::create(builder_, loc, bits1_ty, lhs_val,
                                         rhs_val)
          .getResult();
    case BO::Greater:
      return nsl::dialect::GtOp::create(builder_, loc, bits1_ty, lhs_val,
                                         rhs_val)
          .getResult();
    case BO::GreaterEqual:
      return nsl::dialect::GeOp::create(builder_, loc, bits1_ty, lhs_val,
                                         rhs_val)
          .getResult();
    case BO::LogicalAnd:
      return nsl::dialect::LandOp::create(builder_, loc, bits1_ty, lhs_val,
                                           rhs_val)
          .getResult();
    case BO::LogicalOr:
      return nsl::dialect::LorOp::create(builder_, loc, bits1_ty, lhs_val,
                                          rhs_val)
          .getResult();
    case BO::Div:
    case BO::Mod:
      // No M4 op for these; soft-fail per FR-010 (Sema would have
      // caught the absence of the lowering at the source layer).
      return {};
    }
    return {};
  }
  if (expr->kind() == ast::UnaryExpr::kKind) {
    // FR-007 row "UnaryExpr → nsl.<op>". Two op-trait classes:
    //   (a) `SameOperandsAndResultType` — `Neg`, `BitNot`. Result
    //       type inferred; `create(loc, sub)`.
    //   (b) Width-1 result (`Pure` only): `LogicalNot` (operand
    //       must be width-1), reductions `ReduceAnd/Or/Xor` (operand
    //       any width). create signature is `create(loc, resultType,
    //       sub)`.
    // Unary `Plus` is a no-op identity (no `nsl.plus` op exists at
    // M4 by design — see NSLOps.td §2.2quinque header) — return the
    // sub Value directly.
    // Negated reductions (`~&`/`~|`/`~^`) are not surfaced as a
    // dedicated UnaryExpr::Op enumerator at the AST layer; they
    // would parse as a `BitNot` of a reduction expression, which
    // decomposes naturally through the visitor.
    const auto *un = static_cast<const ast::UnaryExpr *>(expr);
    auto sub_val = lowerExpr(un->sub());
    if (!sub_val) {
      return {};
    }
    auto loc = builder_.getUnknownLoc();
    auto bits1_ty = nsl::dialect::BitsType::get(&ctx_, /*width=*/1);
    using UO = ast::UnaryExpr::Op;
    switch (un->op()) {
    case UO::Plus:
      return sub_val; // identity
    case UO::Neg:
      return nsl::dialect::NegOp::create(builder_, loc, sub_val).getResult();
    case UO::BitNot:
      return nsl::dialect::NotOp::create(builder_, loc, sub_val).getResult();
    case UO::LogicalNot:
      return nsl::dialect::LnotOp::create(builder_, loc, bits1_ty, sub_val)
          .getResult();
    case UO::ReduceAnd:
      return nsl::dialect::ReduceAndOp::create(builder_, loc, bits1_ty,
                                                sub_val)
          .getResult();
    case UO::ReduceOr:
      return nsl::dialect::ReduceOrOp::create(builder_, loc, bits1_ty,
                                               sub_val)
          .getResult();
    case UO::ReduceXor:
      return nsl::dialect::ReduceXorOp::create(builder_, loc, bits1_ty,
                                                sub_val)
          .getResult();
    }
    return {};
  }
  if (expr->kind() == ast::SignExtendExpr::kKind) {
    // FR-007 row "SignExtendExpr → nsl.sign_extend". Width is read
    // from the prefix Decimal literal (S15 spec: width is a
    // compile-time constant); operand width comes from the sub
    // expression's result type.
    const auto *sx = static_cast<const ast::SignExtendExpr *>(expr);
    auto sub_val = lowerExpr(sx->sub());
    if (!sub_val) {
      return {};
    }
    auto loc = builder_.getUnknownLoc();
    auto result_ty =
        nsl::dialect::BitsType::get(&ctx_, resolveWidth(sx->width()));
    return nsl::dialect::SignExtendOp::create(builder_, loc, result_ty, sub_val)
        .getResult();
  }
  if (expr->kind() == ast::ZeroExtendExpr::kKind) {
    // FR-007 row "ZeroExtendExpr → nsl.zero_extend".
    const auto *zx = static_cast<const ast::ZeroExtendExpr *>(expr);
    auto sub_val = lowerExpr(zx->sub());
    if (!sub_val) {
      return {};
    }
    auto loc = builder_.getUnknownLoc();
    auto result_ty =
        nsl::dialect::BitsType::get(&ctx_, resolveWidth(zx->width()));
    return nsl::dialect::ZeroExtendOp::create(builder_, loc, result_ty, sub_val)
        .getResult();
  }
  if (expr->kind() == ast::ConditionalExpr::kKind) {
    // FR-007 row "ConditionalExpr → nsl.mux". Per S14 the else
    // branch is mandatory in expression position; the parser
    // enforces that, so both `thenE` and `elseE` are non-null on
    // any Sema-clean input (FR-010). nsl.mux carries
    // SameOperandsAndResultType for the then/else operands; we
    // pick `thenE`'s lowered type as the result type.
    const auto *cond = static_cast<const ast::ConditionalExpr *>(expr);
    auto cond_val = lowerExpr(cond->cond());
    auto then_val = lowerExpr(cond->thenE());
    auto else_val = lowerExpr(cond->elseE());
    if (!cond_val || !then_val || !else_val) {
      return {};
    }
    auto loc = builder_.getUnknownLoc();
    return nsl::dialect::MuxOp::create(builder_, loc, then_val.getType(),
                                        cond_val, then_val, else_val)
        .getResult();
  }
  if (expr->kind() == ast::SliceExpr::kKind) {
    // FR-007 row "SliceExpr → nsl.extract". Per S15 the indices
    // are compile-time constants (Sema's domain); at the AST level
    // they are LiteralExpr Decimals. Single-index form
    // (`v[i]`) has `lo == nullptr` (per SliceExpr.h doc) — that
    // collapses to `lowBit = i`, `width = 1`.
    const auto *sl = static_cast<const ast::SliceExpr *>(expr);
    auto sub_val = lowerExpr(sl->sub());
    if (!sub_val) {
      return {};
    }
    int64_t hi = resolveDecimalLiteral(sl->hi());
    int64_t lo = sl->lo() ? resolveDecimalLiteral(sl->lo()) : hi;
    if (hi < lo) {
      return {};
    }
    int64_t low_bit = lo;
    unsigned width = static_cast<unsigned>(hi - lo + 1);
    auto loc = builder_.getUnknownLoc();
    auto result_ty = nsl::dialect::BitsType::get(&ctx_, width);
    return nsl::dialect::ExtractOp::create(builder_, loc, result_ty, sub_val,
                                            static_cast<uint64_t>(low_bit))
        .getResult();
  }
  if (expr->kind() == ast::ConcatExpr::kKind) {
    // FR-007 row "ConcatExpr → nsl.concat". Operand order is the
    // NSL source's left-to-right MSB-first convention (per S18 +
    // §11 line 698 — the CIRCT `comb.concat` convention as well).
    // Result width is the sum of operand widths read off each
    // operand's `BitsType`.
    const auto *cc = static_cast<const ast::ConcatExpr *>(expr);
    llvm::SmallVector<mlir::Value, 4> part_vals;
    part_vals.reserve(cc->parts().size());
    unsigned total_width = 0;
    for (const auto &p : cc->parts()) {
      auto v = lowerExpr(p.get());
      if (!v) {
        return {};
      }
      auto bits_ty = mlir::dyn_cast<nsl::dialect::BitsType>(v.getType());
      if (!bits_ty) {
        return {};
      }
      total_width += bits_ty.getWidth();
      part_vals.push_back(v);
    }
    if (part_vals.empty()) {
      return {};
    }
    auto loc = builder_.getUnknownLoc();
    auto result_ty = nsl::dialect::BitsType::get(&ctx_, total_width);
    return nsl::dialect::ConcatOp::create(builder_, loc, result_ty,
                                           mlir::ValueRange(part_vals))
        .getResult();
  }
  if (expr->kind() == ast::StructCastExpr::kKind) {
    // FR-007 row "StructCastExpr → nsl.struct_cast + nsl.field chain".
    // The NSL grammar (`lang.ebnf §11`) requires at least one
    // `.member` after `(T)(x)` (parser enforces this at
    // `lib/Parse/ParseExpr.cpp:222-235`), so `memberPath()` is never
    // empty on a Sema-clean input (FR-010).
    //
    // Lowering shape (per data-model + Q3 → Option A on M4 cast strict
    // type-equality): emit a single `nsl.struct_cast %sub : !nsl.bits<W>
    // to !nsl.struct<@T>` then chain `nsl.field` per memberPath
    // element. Each `nsl.field` walks one level into the struct;
    // nested struct-typed fields are exercised by the M5 expansion
    // pass downstream — this Phase B increment ships only the
    // single-level shape covered by the smoke fixture.
    const auto *sc = static_cast<const ast::StructCastExpr *>(expr);
    auto sub_val = lowerExpr(sc->sub());
    if (!sub_val) {
      return {};
    }
    if (sc->memberPath().empty()) {
      // Defensive: parser guarantees ≥1, but the cast op alone with
      // no field access would still lower legally — emit just the
      // struct_cast in that case.
      auto loc = builder_.getUnknownLoc();
      auto sym_ref = mlir::SymbolRefAttr::get(
          builder_.getStringAttr(sc->typeName()));
      auto struct_ty = nsl::dialect::StructType::get(&ctx_, sym_ref);
      return nsl::dialect::StructCastOp::create(builder_, loc, struct_ty,
                                                 sub_val)
          .getResult();
    }
    // Look up the struct in the transitional name-keyed catalog.
    auto struct_it = structTable_.find(sc->typeName());
    if (struct_it == structTable_.end()) {
      // No matching `nsl.struct` declared yet — Sema-clean inputs
      // would have flagged the missing type upstream (FR-010);
      // soft-fail per the offload decision.
      return {};
    }
    auto loc = builder_.getUnknownLoc();
    auto sym_ref = mlir::SymbolRefAttr::get(
        builder_.getStringAttr(sc->typeName()));
    auto struct_ty = nsl::dialect::StructType::get(&ctx_, sym_ref);
    mlir::Value cur = nsl::dialect::StructCastOp::create(
                          builder_, loc, struct_ty, sub_val)
                          .getResult();
    // Chain `nsl.field` per memberPath segment. Phase B handles only
    // the single-level case; chained struct-typed fields would
    // require recursing into the field's type's structTable_ entry.
    const auto *cur_fields = &struct_it->second;
    for (const ast::Identifier &member : sc->memberPath()) {
      // Linear scan is fine: structs are typically tiny (≤ 8 fields).
      auto field_it = std::find_if(
          cur_fields->begin(), cur_fields->end(),
          [&](const StructField &f) { return f.name == member; });
      if (field_it == cur_fields->end()) {
        return {}; // Sema-clean inputs wouldn't reach here.
      }
      auto idx = static_cast<uint64_t>(field_it - cur_fields->begin());
      mlir::Type field_ty = field_it->type;
      cur = nsl::dialect::FieldOp::create(builder_, loc, field_ty, cur, idx)
                .getResult();
      // Walk into a nested struct if the field's type is itself a
      // struct (so the next memberPath element resolves correctly).
      if (auto next_struct = mlir::dyn_cast<nsl::dialect::StructType>(field_ty)) {
        auto nested_it = structTable_.find(
            next_struct.getName().getRootReference().getValue());
        if (nested_it == structTable_.end()) {
          return {};
        }
        cur_fields = &nested_it->second;
      }
    }
    return cur;
  }
  if (expr->kind() == ast::FieldAccessExpr::kKind) {
    // FR-007 row "FieldAccessExpr → nsl.field". The `obj` Expr must
    // resolve to an `mlir::Value` of `!nsl.struct<@T>` type — the
    // sole shape Phase B exercises here is `<inst>.field` where
    // `inst` is an `ast::StructInstDecl`-emitted `nsl.reg`/`nsl.wire`
    // of struct type, registered in `nameTable_`. Submodule-port
    // access (`cpu_t inst; inst.port`) shares the same AST node but
    // resolves to a non-struct Value (or no Value at all in the
    // Phase B implementation); soft-fail per FR-010 — the M5
    // structural-expansion pass plus M3 Sema type-resolution will
    // disambiguate downstream.
    const auto *fa = static_cast<const ast::FieldAccessExpr *>(expr);
    auto obj_val = lowerExpr(fa->obj());
    if (!obj_val) {
      return {};
    }
    auto struct_ty =
        mlir::dyn_cast<nsl::dialect::StructType>(obj_val.getType());
    if (!struct_ty) {
      // Non-struct `.field` access (likely a submodule port) —
      // outside Phase B scope.
      return {};
    }
    llvm::StringRef struct_name =
        struct_ty.getName().getRootReference().getValue();
    auto struct_it = structTable_.find(struct_name);
    if (struct_it == structTable_.end()) {
      return {};
    }
    const auto &fields = struct_it->second;
    auto field_it = std::find_if(
        fields.begin(), fields.end(),
        [&](const StructField &f) { return f.name == fa->field(); });
    if (field_it == fields.end()) {
      return {};
    }
    auto idx = static_cast<uint64_t>(field_it - fields.begin());
    mlir::Type field_ty = field_it->type;
    auto loc = builder_.getUnknownLoc();
    return nsl::dialect::FieldOp::create(builder_, loc, field_ty, obj_val, idx)
        .getResult();
  }
  // Other expression-position kinds (Repeat/Call/IncDec/SystemVar) —
  // T055.
  return {};
}

void ASTToMLIR::visit(const ast::LiteralExpr &node) {
  // Visitor entry-point form. Most expression-position lowering goes
  // through `lowerExpr`, but this override exists to satisfy the
  // ASTVisitor pure-virtual contract; it produces no IR at the
  // current insertion point (literals are pure values, not
  // statements). Callers that need an `mlir::Value` invoke
  // `lowerExpr(&node)` instead.
  (void)lowerExpr(&node);
}

void ASTToMLIR::visit(const ast::IdentifierExpr &node) {
  // Visitor entry-point form (cf. `visit(LiteralExpr)`). Pure value
  // — no IR emitted at the current insertion point.
  (void)lowerExpr(&node);
}

void ASTToMLIR::visit(const ast::BinaryExpr &node) {
  // Visitor entry-point form (cf. `visit(LiteralExpr)`). The actual
  // dispatch into the per-operator `nsl.<op>` lowering is in
  // `lowerExpr` — callers wanting an `mlir::Value` route through it.
  (void)lowerExpr(&node);
}

void ASTToMLIR::visit(const ast::UnaryExpr &node) {
  (void)lowerExpr(&node);
}

void ASTToMLIR::visit(const ast::SignExtendExpr &node) {
  (void)lowerExpr(&node);
}

void ASTToMLIR::visit(const ast::ZeroExtendExpr &node) {
  (void)lowerExpr(&node);
}

void ASTToMLIR::visit(const ast::ConditionalExpr &node) {
  (void)lowerExpr(&node);
}

void ASTToMLIR::visit(const ast::SliceExpr &node) {
  (void)lowerExpr(&node);
}

void ASTToMLIR::visit(const ast::ConcatExpr &node) {
  (void)lowerExpr(&node);
}

void ASTToMLIR::visit(const ast::StructCastExpr &node) {
  (void)lowerExpr(&node);
}

void ASTToMLIR::visit(const ast::FieldAccessExpr &node) {
  (void)lowerExpr(&node);
}

// ---------- Struct-related decl visitors ----------

void ASTToMLIR::visit(const ast::StructDecl &node) {
  // FR-006 row "StructDecl → nsl.struct @<name> { nsl.field_decl ... }".
  // Per post-merge M4-amendment 2026-05-02 (#3) the parent of
  // `nsl.struct` is `ParentOneOf<["::mlir::ModuleOp", "ModuleOp"]>`,
  // so emit at the current insertion point — top-level placement
  // (sibling of `nsl.module` under the builtin ModuleOp) and nested
  // placement inside `nsl.module` are both legal. Field types come
  // from each member's width expression (`resolveWidth` — Phase 3
  // covers Decimal-literal widths only; richer width resolution is
  // T055 follow-up). Struct-typed fields are deferred to a future
  // increment (the Phase B smoke fixtures use bits-only fields).
  auto loc = builder_.getUnknownLoc();
  auto struct_op = nsl::dialect::StructOp::create(
      builder_, loc, builder_.getStringAttr(node.name()));
  auto &body_block = struct_op.getBody().emplaceBlock();

  // Populate the transitional name-keyed catalog as we emit each
  // `nsl.field_decl`. Iteration order = source-declaration order =
  // MSB-first per S18 (`lang.ebnf:889`).
  llvm::SmallVector<StructField, 4> fields;
  fields.reserve(node.members().size());
  {
    mlir::OpBuilder::InsertionGuard guard(builder_);
    builder_.setInsertionPointToStart(&body_block);
    for (const auto &member : node.members()) {
      auto field_ty =
          nsl::dialect::BitsType::get(&ctx_, resolveWidth(member.width.get()));
      (void)nsl::dialect::FieldDeclOp::create(
          builder_, loc, builder_.getStringAttr(member.name),
          mlir::TypeAttr::get(field_ty));
      fields.push_back({member.name, field_ty});
    }
  }
  structTable_[node.name()] = std::move(fields);
}

void ASTToMLIR::visit(const ast::StructInstDecl &node) {
  // FR-006 row "StructInstDecl → nsl.reg \"name\" : !nsl.struct<@T>"
  // (Reg form) or "nsl.wire \"name\" : !nsl.struct<@T>" (Wire form).
  // Phase B ships scalar instances only; array form (`arraySize`) and
  // initializer form (`init`) are deferred to a follow-up. The struct
  // type is referenced symbolically via `!nsl.struct<@TypeName>` —
  // MLIR's SymbolTable resolution walks to the enclosing
  // `mlir::ModuleOp` and finds the sibling `nsl.struct` decl
  // (top-level or nested both supported per amendment #3).
  auto loc = builder_.getUnknownLoc();
  auto sym_ref =
      mlir::SymbolRefAttr::get(builder_.getStringAttr(node.typeName()));
  auto struct_ty = nsl::dialect::StructType::get(&ctx_, sym_ref);

  if (node.storageKind() == ast::StructInstDecl::StorageKind::Reg) {
    // `nsl.reg` accepts `!nsl.struct<@T>` per FR-013 + the existing
    // round-trip fixture `test/Dialect/Types/struct_roundtrip.mlir`.
    // No init attribute at Phase B (struct-init lowering is a
    // follow-up).
    auto reg_op = nsl::dialect::RegOp::create(
        builder_, loc, struct_ty,
        builder_.getStringAttr(node.instanceName()),
        /*init=*/mlir::IntegerAttr{});
    nameTable_[node.instanceName()] = reg_op.getResult();
    return;
  }
  // Wire form. NOTE: `nsl.wire` carries `NSL_AnyBits` on its result,
  // so a struct-typed wire would be rejected by the dialect verifier
  // (per FR-013 row "wires never carry !nsl.struct<@T>"). Emit nothing
  // — the Phase B smoke fixture exercises the Reg form only.
  // Sema-clean inputs at M3 will flag this case upstream (S-rule TBD).
}

// ---------- Statement-position visitors (cont'd) ----------

void ASTToMLIR::visit(const ast::TransferStmt &node) {
  // FR-006 rows "TransferStmt × `=` → nsl.transfer" and "TransferStmt
  // × `:=` → nsl.clocked_transfer". Both ops carry `SameTypeOperands`
  // (TableGen NSL_TransferOp / NSL_ClockedTransferOp), so the LHS
  // and RHS Values must agree on `!nsl.bits<N>` width — this is
  // load-bearing for round-trip cleanliness. Phase 3 inputs are
  // Sema-clean per FR-010, so the AST-level S15 width-agreement
  // invariant has already been established upstream.
  auto lhs_val = lowerExpr(node.lhs());
  auto rhs_val = lowerExpr(node.rhs());
  if (!lhs_val || !rhs_val) {
    // Soft-fail per FR-010 — Sema would have caught unresolved
    // references. Emit nothing rather than a malformed op.
    return;
  }
  auto loc = builder_.getUnknownLoc();
  if (node.op() == ast::TransferStmt::Op::RegColonEq) {
    (void)nsl::dialect::ClockedTransferOp::create(builder_, loc, lhs_val,
                                                  rhs_val);
    return;
  }
  // Op::WireEq — `=`.
  (void)nsl::dialect::TransferOp::create(builder_, loc, lhs_val, rhs_val);
}

void ASTToMLIR::visit(const ast::ControlCallStmt &node) {
  // FR-006 row "ControlCallStmt → nsl.call @target(args...)". The
  // ScopedName lowers per Q5 Option A' as a literal-dotted
  // `FlatSymbolRefAttr` (e.g., `inst.start` → `@inst.start`); no
  // name mangling. The callee may not have been visited yet — Q4
  // Option A defers symbol-resolution to MLIR's SymbolTable, so
  // the FlatSymbolRefAttr is correct even if the target op lands
  // later in the walk.
  //
  // Phase 3 expression-sub-visitor coverage is partial (LiteralExpr
  // Decimal + IdentifierExpr single-part only). Args that fail to
  // lower (`!val`) soft-fail per FR-010 — Sema-clean inputs would
  // already have rejected the bad arg shape. Result types: Phase 3
  // emits no results (the M3 corpus exercises void control-calls);
  // returning calls land alongside CallExpr (T055).
  auto loc = builder_.getUnknownLoc();
  std::string flat_name;
  llvm::raw_string_ostream os(flat_name);
  for (size_t i = 0; i < node.target().parts.size(); ++i) {
    if (i > 0) {
      os << '.';
    }
    os << node.target().parts[i];
  }
  os.flush();
  auto callee = mlir::FlatSymbolRefAttr::get(builder_.getStringAttr(flat_name));
  llvm::SmallVector<mlir::Value, 4> arg_vals;
  arg_vals.reserve(node.args().size());
  for (const auto &arg : node.args()) {
    auto v = lowerExpr(arg.get());
    if (!v) {
      // Soft-fail: any arg we cannot lower (Phase 3 expression
      // surface gap) bails the whole call. Sema would have caught
      // unresolved refs upstream; this only fires on future-feature
      // gaps.
      return;
    }
    arg_vals.push_back(v);
  }
  (void)nsl::dialect::CallOp::create(builder_, loc,
                                     /*results=*/mlir::TypeRange{}, callee,
                                     mlir::ValueRange(arg_vals));
}

// ---------- Action-block stmt-position visitors ----------

// Shared body for `visit(AltBlock)` / `visit(AnyBlock)` — both AST
// nodes have identical shape (`vector<CondCase>` + nullable
// elseCase). Caller has already created the parent (`nsl.alt` /
// `nsl.any`) and placed the builder's insertion point at the start
// of the parent's body block. Each `nsl.case` is a child op of the
// parent — both ops carry `ParentOneOf<["AltOp", "AnyOp"]>`
// (NSLOps.td §2.5).
//
// We declare this as a regular member-function-like inlinable lambda
// inside each visitor body to avoid pollution of the header / member
// surface; visitor-private state (`lowerExpr`, `lowerActionBody`)
// remains directly in scope.

void ASTToMLIR::visit(const ast::AltBlock &node) {
  // FR-006 row "AltBlock → nsl.alt { nsl.case ... nsl.default ... }"
  // (design §7 line 903). `nsl.alt` carries no `HasParent`
  // constraint; per S13 it's constructive (priority vs parallel) and
  // the dialect verifier does NOT distinguish alt-vs-any semantics —
  // the introspection observable lives in M3.
  auto loc = builder_.getUnknownLoc();
  auto alt_op = nsl::dialect::AltOp::create(builder_, loc);
  auto &body_block = alt_op.getBody().emplaceBlock();
  mlir::OpBuilder::InsertionGuard guard(builder_);
  builder_.setInsertionPointToStart(&body_block);
  for (const auto &arm : node.cases()) {
    auto cond_val = lowerExpr(arm.cond.get());
    if (!cond_val) {
      continue;
    }
    auto case_op = nsl::dialect::CaseOp::create(builder_, loc, cond_val);
    auto &case_body = case_op.getBody().emplaceBlock();
    mlir::OpBuilder::InsertionGuard caseGuard(builder_);
    builder_.setInsertionPointToStart(&case_body);
    lowerActionBody(arm.body.get());
  }
  if (node.elseCase()) {
    auto default_op = nsl::dialect::DefaultOp::create(builder_, loc);
    auto &default_body = default_op.getBody().emplaceBlock();
    mlir::OpBuilder::InsertionGuard defaultGuard(builder_);
    builder_.setInsertionPointToStart(&default_body);
    lowerActionBody(node.elseCase());
  }
}

void ASTToMLIR::visit(const ast::AnyBlock &node) {
  // FR-006 row "AnyBlock → nsl.any { nsl.case ... }" (design §7
  // line 904). Identical shape to `AltBlock`, distinct op kind.
  // Children = `nsl.case` and `nsl.default` (same as alt — the
  // `ParentOneOf` constraint accepts both parents).
  auto loc = builder_.getUnknownLoc();
  auto any_op = nsl::dialect::AnyOp::create(builder_, loc);
  auto &body_block = any_op.getBody().emplaceBlock();
  mlir::OpBuilder::InsertionGuard guard(builder_);
  builder_.setInsertionPointToStart(&body_block);
  for (const auto &arm : node.cases()) {
    auto cond_val = lowerExpr(arm.cond.get());
    if (!cond_val) {
      continue;
    }
    auto case_op = nsl::dialect::CaseOp::create(builder_, loc, cond_val);
    auto &case_body = case_op.getBody().emplaceBlock();
    mlir::OpBuilder::InsertionGuard caseGuard(builder_);
    builder_.setInsertionPointToStart(&case_body);
    lowerActionBody(arm.body.get());
  }
  if (node.elseCase()) {
    auto default_op = nsl::dialect::DefaultOp::create(builder_, loc);
    auto &default_body = default_op.getBody().emplaceBlock();
    mlir::OpBuilder::InsertionGuard defaultGuard(builder_);
    builder_.setInsertionPointToStart(&default_body);
    lowerActionBody(node.elseCase());
  }
}

void ASTToMLIR::visit(const ast::IfStmt &node) {
  // FR-006 row "IfStmt → nsl.if %cond { ... } else { ... }" (design
  // §7 line 905). `nsl.if` has two `SizedRegion<1>`s; both are
  // structurally required by the M4 op definition even when the
  // source `else` arm is omitted — we materialise an empty else
  // block in that case, mirroring the round-trip fixture
  // `test/Dialect/action-block/if_roundtrip.mlir` (`{ } else { }`).
  auto cond_val = lowerExpr(node.cond());
  if (!cond_val) {
    // Soft-fail per FR-010 — Sema-clean inputs would have caught
    // unresolved/unsupported cond expressions upstream.
    return;
  }
  auto loc = builder_.getUnknownLoc();
  auto if_op = nsl::dialect::IfOp::create(builder_, loc, cond_val);
  auto &then_block = if_op.getThenRegion().emplaceBlock();
  auto &else_block = if_op.getElseRegion().emplaceBlock();
  {
    mlir::OpBuilder::InsertionGuard guard(builder_);
    builder_.setInsertionPointToStart(&then_block);
    lowerActionBody(node.thenBr());
  }
  if (node.elseBr()) {
    mlir::OpBuilder::InsertionGuard guard(builder_);
    builder_.setInsertionPointToStart(&else_block);
    lowerActionBody(node.elseBr());
  }
  // Empty else region is intentional when AST elseBr is null; the
  // structural slot remains a single empty block.
}

void ASTToMLIR::visit(const ast::WhileBlock &node) {
  // FR-006 row "WhileBlock → nsl.while %cond { ... }" (design §7
  // line 908). Per Q2 Option B `nsl.while` requires a transitive
  // `nsl.seq` ancestor (verifier in `NSLOps.cpp:563`); the M3
  // grammar's `S8` constraint (while/for must live inside `seq`)
  // ensures Sema-clean inputs always supply that ancestor.
  auto cond_val = lowerExpr(node.cond());
  if (!cond_val) {
    return;
  }
  auto loc = builder_.getUnknownLoc();
  auto while_op = nsl::dialect::WhileOp::create(builder_, loc, cond_val);
  auto &body_block = while_op.getBody().emplaceBlock();
  mlir::OpBuilder::InsertionGuard guard(builder_);
  builder_.setInsertionPointToStart(&body_block);
  for (const auto &item : node.items()) {
    item->accept(*this);
  }
}

void ASTToMLIR::visit(const ast::ForBlock &node) {
  // FR-006 row "ForBlock (C-style form) → nsl.for %init, %cond,
  // %step { ... }" (design §7 line 909). Per Q2 Option B `nsl.for`
  // requires a transitive `nsl.seq` ancestor (verifier in
  // `NSLOps.cpp:570`); per S8 Sema-clean inputs supply that
  // ancestor.
  //
  // Phase 3 scope (offload 2026-04-30): C-form ONLY — init/cond/step
  // all present. The init/step Stmts' carried target identifier is
  // resolved through `nameTable_` to produce the `%init`/`%step`
  // operand Values (matching the M4 round-trip fixture
  // `test/Dialect/action-block/for_roundtrip.mlir` shape — opaque
  // wire Values supplied to the marker op).
  //
  // ENUM-FORM (offload 2026-04-30 escalation): the spec
  // (`spec.md:419`) maps `for (i = 0..N) { ... }` to a 2-operand
  // `nsl.for %init, %end { ... }`, but the M4 op surface (frozen at
  // 77, per offload constraint) defines `nsl.for` with **three**
  // operands (`init, cond, step`). No clean mapping at Phase 3 — the
  // enum form falls through to soft-fail. Resolution is either an
  // M4 op-surface amendment (out of scope) or a Sema-injected
  // synthetic step (M3 territory). Tracked in the offload report.
  const ast::ForForm &form = node.form();
  if (!form.init || !form.cond || !form.step) {
    // Enum form (step == nullptr) or malformed — soft-fail per
    // FR-010. Sema-clean inputs at M3 will eventually disambiguate.
    return;
  }
  // Resolve the init Stmt's loop-variable target. Init is either a
  // `TransferStmt` (`id := expr`, parser line 798) or a
  // `ParallelBlock` (compound init, parser line 779). Phase 3 only
  // handles the simple TransferStmt shape; compound init is
  // soft-fail.
  const ast::Expr *init_target = nullptr;
  if (form.init->kind() == ast::TransferStmt::kKind) {
    init_target = static_cast<const ast::TransferStmt *>(form.init.get())->lhs();
  }
  // Resolve the step Stmt's loop-variable target. Step is either an
  // `IncDecStmt` (`id++`/`++id`/...) or a `TransferStmt` (`id :=
  // expr`).
  const ast::Expr *step_target = nullptr;
  if (form.step->kind() == ast::TransferStmt::kKind) {
    step_target = static_cast<const ast::TransferStmt *>(form.step.get())->lhs();
  } else if (form.step->kind() == ast::IncDecStmt::kKind) {
    step_target =
        static_cast<const ast::IncDecStmt *>(form.step.get())->target();
  }
  if (!init_target || !step_target) {
    return;
  }
  auto init_val = lowerExpr(init_target);
  auto cond_val = lowerExpr(form.cond.get());
  auto step_val = lowerExpr(step_target);
  if (!init_val || !cond_val || !step_val) {
    return;
  }
  auto loc = builder_.getUnknownLoc();
  auto for_op = nsl::dialect::ForOp::create(builder_, loc, init_val, cond_val,
                                             step_val);
  auto &body_block = for_op.getBody().emplaceBlock();
  mlir::OpBuilder::InsertionGuard guard(builder_);
  builder_.setInsertionPointToStart(&body_block);
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

STUB(TopLevelParamDecl)
STUB(DeclareBlock)
STUB(PortDecl)
STUB(VariableDecl)
STUB(IntegerDecl)
STUB(FuncSelfDecl)
STUB(ProcNameDecl)
STUB(StateNameDecl)
STUB(SubmoduleDecl)

STUB(IncDecStmt)
STUB(ReturnStmt)
STUB(EmptyStmt)
STUB(LabeledStmt)
STUB(GotoStmt)
STUB(StructuralGenerate)

STUB(SystemVarExpr)
STUB(RepeatExpr)
STUB(CallExpr)
STUB(IncDecExpr)

#undef STUB

} // namespace nsl::lower
