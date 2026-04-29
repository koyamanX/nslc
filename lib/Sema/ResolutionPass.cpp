// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// lib/Sema/ResolutionPass.cpp — the single top-down `ASTVisitor`
// walk of the M3 Sema pass. Implements:
//
//   1. Scope handling — `enterScope` / `leaveScope` per data-model
//      §2.1's six-kind enum (Global / Declare / Module / Proc /
//      SeqOrParallel / Function).
//
//   2. Symbol declaration — at every declaring AST node (RegDecl,
//      WireDecl, etc. — 13 kinds per data-model §1.5), the pass
//      constructs the matching `Symbol` subclass and calls
//      `SymbolTable::declare`. Same-scope duplicates produce a
//      single "duplicate name" diagnostic.
//
//   3. Name resolution — `IdentifierExpr` / `FieldAccessExpr` /
//      `ScopedName` heads call `SymbolTable::lookup` /
//      `lookupScoped`. The resolved `Symbol*` is recorded in the
//      `ResolutionMap`'s `exprToSymbol` side-table for printer
//      consumption (per data-model §6 / `emit-ast-format.contract.md`
//      Invariant 3). Unresolved names emit exactly ONE diagnostic
//      per distinct name (FR-017 no-cascade) and tag the Expr's
//      `inferredType()` as `unresolvedSingleton()`.
//
//   4. Width inference — for every `Expr` reached, the pass writes
//      a non-null `TypeRef` to `Expr::setInferredType()`. Algorithm
//      per design §6.x line 856 ("Width inference is a single
//      top-down pass"):
//        - Inherent-width forms: `LiteralExpr` (decimal → smallest
//          BitVector that fits; binary `5'b10101` → BitVector(5));
//          `SliceExpr` (hi-lo+1, M3 stub uses a placeholder);
//          `SignExtendExpr`/`ZeroExtendExpr` (explicit width);
//          `IdentifierExpr` (resolved Symbol's type);
//          `StructCastExpr` (struct's totalWidth).
//        - Context-dependent forms: `BinaryExpr`, `ConditionalExpr`,
//          `ConcatExpr`, `RepeatExpr`, `CallExpr`, `UnaryExpr`,
//          `IncDecExpr`. M3 uses a context-width stack pushed on
//          `TransferStmt::lhs` width (when known) and propagated
//          to the RHS subtree.
//
//   5. No-cascade — a `DenseSet<StringRef> reportedUnresolved`
//      ensures duplicate use sites of the same unresolved name
//      produce exactly one diagnostic per `sema-stability.contract.md`
//      Invariant 6.

#include "ResolutionPass.h"

#include "nsl/AST/AltBlock.h"
#include "nsl/AST/AnyBlock.h"
#include "nsl/AST/BareFinishStmt.h"
#include "nsl/AST/BinaryExpr.h"
#include "nsl/AST/CallExpr.h"
#include "nsl/AST/CompilationUnit.h"
#include "nsl/AST/ConcatExpr.h"
#include "nsl/AST/ConditionalExpr.h"
#include "nsl/AST/ControlCallStmt.h"
#include "nsl/AST/DeclareBlock.h"
#include "nsl/AST/DelayTaskStmt.h"
#include "nsl/AST/EmptyStmt.h"
#include "nsl/AST/Expr.h"
#include "nsl/AST/FieldAccessExpr.h"
#include "nsl/AST/FirstStateDecl.h"
#include "nsl/AST/ForBlock.h"
#include "nsl/AST/FuncDefn.h"
#include "nsl/AST/FuncSelfDecl.h"
#include "nsl/AST/GotoStmt.h"
#include "nsl/AST/IdentifierExpr.h"
#include "nsl/AST/IfStmt.h"
#include "nsl/AST/IncDecExpr.h"
#include "nsl/AST/IncDecStmt.h"
#include "nsl/AST/InitBlockStmt.h"
#include "nsl/AST/IntegerDecl.h"
#include "nsl/AST/LabeledStmt.h"
#include "nsl/AST/LiteralExpr.h"
#include "nsl/AST/MemDecl.h"
#include "nsl/AST/ModuleBlock.h"
#include "nsl/AST/ParallelBlock.h"
#include "nsl/AST/PortDecl.h"
#include "nsl/AST/ProcDefn.h"
#include "nsl/AST/ProcNameDecl.h"
#include "nsl/AST/RegDecl.h"
#include "nsl/AST/RepeatExpr.h"
#include "nsl/AST/ReturnStmt.h"
#include "nsl/AST/SeqBlock.h"
#include "nsl/AST/SignExtendExpr.h"
#include "nsl/AST/SliceExpr.h"
#include "nsl/AST/StateDefn.h"
#include "nsl/AST/StateNameDecl.h"
#include "nsl/AST/Stmt.h"
#include "nsl/AST/StructCastExpr.h"
#include "nsl/AST/StructDecl.h"
#include "nsl/AST/StructInstDecl.h"
#include "nsl/AST/StructuralGenerate.h"
#include "nsl/AST/SubmoduleDecl.h"
#include "nsl/AST/SystemTaskStmt.h"
#include "nsl/AST/SystemVarExpr.h"
#include "nsl/AST/TopLevelParamDecl.h"
#include "nsl/AST/TransferStmt.h"
#include "nsl/AST/UnaryExpr.h"
#include "nsl/AST/VariableDecl.h"
#include "nsl/AST/WhileBlock.h"
#include "nsl/AST/WireDecl.h"
#include "nsl/AST/ZeroExtendExpr.h"
#include "nsl/Basic/Diagnostic.h"
#include "nsl/Basic/SourceLocation.h"
#include "nsl/Sema/SymbolTable.h"
#include "nsl/Sema/TypeSystem.h"

#include "llvm/ADT/DenseSet.h"
#include "llvm/ADT/StringMap.h"
#include "llvm/ADT/StringRef.h"

#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace nsl::sema {

namespace {

// =====================================================================
// Helpers — literal-width inference (FR-009)
// =====================================================================

/// Compute the minimum number of bits needed to represent `v` as an
/// unsigned BitVector. `v == 0` → 1 (the convention for the
/// 1-bit zero literal). The result is always `>= 1`.
uint64_t minBits(uint64_t v) noexcept {
  if (v == 0) {
    return 1;
  }
  uint64_t bits = 0;
  while (v != 0) {
    ++bits;
    v >>= 1U;
  }
  return bits;
}

/// Parse a decimal literal spelling like `"123"` into a `uint64_t`.
/// Returns 0 on parse failure (keeps the inference stable — the
/// caller falls back to a minimum-width assumption).
uint64_t parseDecimal(llvm::StringRef s) noexcept {
  uint64_t v = 0;
  for (char c : s) {
    if (c == '_') {
      continue;
    }
    if (c < '0' || c > '9') {
      return 0;
    }
    v = v * 10U + static_cast<uint64_t>(c - '0');
  }
  return v;
}

/// Parse a hex literal spelling (no `0x` prefix; e.g., `"FF"`).
uint64_t parseHex(llvm::StringRef s) noexcept {
  uint64_t v = 0;
  // Skip a leading "0x" / "0X" if present.
  if (s.size() >= 2 && s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) {
    s = s.substr(2);
  }
  for (char c : s) {
    if (c == '_') {
      continue;
    }
    uint64_t d = 0;
    if (c >= '0' && c <= '9') {
      d = static_cast<uint64_t>(c - '0');
    } else if (c >= 'a' && c <= 'f') {
      d = static_cast<uint64_t>(10 + (c - 'a'));
    } else if (c >= 'A' && c <= 'F') {
      d = static_cast<uint64_t>(10 + (c - 'A'));
    } else {
      return 0;
    }
    v = (v << 4U) | d;
  }
  return v;
}

/// Parse an NSL-flavored sized binary literal like `"5'b10101"` and
/// return `(width, value)`. Returns `(0, 0)` if the spelling is not
/// the sized form. The lexer reports `Lit::Binary` for `'b...` even
/// when the leading width is missing — in that case, count the
/// significant bits.
std::pair<uint64_t, uint64_t> parseBinary(llvm::StringRef s) noexcept {
  // Find a `'` if present.
  std::size_t apos = llvm::StringRef::npos;
  for (std::size_t i = 0; i < s.size(); ++i) {
    if (s[i] == '\'') {
      apos = i;
      break;
    }
  }
  uint64_t width = 0;
  llvm::StringRef body;
  if (apos != llvm::StringRef::npos) {
    width = parseDecimal(s.substr(0, apos));
    body = s.substr(apos + 1);
    // Skip the `b` / `B` base char.
    if (!body.empty() && (body[0] == 'b' || body[0] == 'B')) {
      body = body.substr(1);
    }
  } else {
    body = s;
  }
  uint64_t v = 0;
  uint64_t bits = 0;
  for (char c : body) {
    if (c == '_') {
      continue;
    }
    if (c == '0') {
      v <<= 1U;
      ++bits;
    } else if (c == '1') {
      v = (v << 1U) | 1U;
      ++bits;
    } else {
      // Z/X/U digits — count as bit but value is opaque.
      v <<= 1U;
      ++bits;
    }
  }
  if (width == 0) {
    width = bits == 0 ? 1 : bits;
  }
  return {width, v};
}

/// Compute the inferred-type width for a `LiteralExpr`. Per FR-009,
/// integer literals resolve to a concrete `BitVectorType{N}`.
TypeRef literalType(const ast::LiteralExpr &lit, TypeSystem &ts,
                    uint64_t contextWidth) noexcept {
  switch (lit.litKind()) {
  case ast::LiteralExpr::Lit::Decimal: {
    uint64_t const v = parseDecimal(lit.spelling());
    uint64_t width = minBits(v);
    // If the surrounding context provides a width, prefer it (NSL
    // bare integer literals adopt the context width per Ref §0).
    if (contextWidth != 0 && contextWidth >= width) {
      width = contextWidth;
    }
    return ts.bitVector(width);
  }
  case ast::LiteralExpr::Lit::Hex: {
    uint64_t const v = parseHex(lit.spelling());
    uint64_t width = minBits(v);
    // Hex literals get rounded up to a multiple-of-4-bit width per
    // the convention in NSL fixtures; if context provides a wider
    // anchor, prefer it.
    width = ((width + 3U) / 4U) * 4U;
    if (width == 0) {
      width = 1;
    }
    if (contextWidth != 0 && contextWidth >= width) {
      width = contextWidth;
    }
    return ts.bitVector(width);
  }
  case ast::LiteralExpr::Lit::Binary: {
    auto wv = parseBinary(lit.spelling());
    uint64_t width = wv.first;
    if (width == 0) {
      width = 1;
    }
    return ts.bitVector(width);
  }
  case ast::LiteralExpr::Lit::Octal: {
    // Same shape as Hex but base-8.
    uint64_t v = 0;
    llvm::StringRef body = lit.spelling();
    // Strip "0o" / "0" prefix if present.
    if (body.size() >= 2 && body[0] == '0' &&
        (body[1] == 'o' || body[1] == 'O')) {
      body = body.substr(2);
    }
    for (char c : body) {
      if (c == '_') {
        continue;
      }
      if (c < '0' || c > '7') {
        v = 0;
        break;
      }
      v = (v << 3U) | static_cast<uint64_t>(c - '0');
    }
    uint64_t width = minBits(v);
    width = ((width + 2U) / 3U) * 3U;
    if (width == 0) {
      width = 1;
    }
    if (contextWidth != 0 && contextWidth >= width) {
      width = contextWidth;
    }
    return ts.bitVector(width);
  }
  case ast::LiteralExpr::Lit::String:
    // String literals don't have a meaningful bit-width at M3.
    // Use BitVector(8 * length) as a placeholder.
    return ts.bitVector(8U * static_cast<uint64_t>(lit.spelling().size()
                                                       ? lit.spelling().size()
                                                       : 1U));
  }
  return ts.unresolved();
}

// =====================================================================
// Helpers — symbol-from-AST construction
// =====================================================================

/// Map a parser `PortDecl::Direction` to the Sema `PortDirection`
/// enum. The two control-terminal directions (FuncIn/FuncOut) don't
/// map to a `PortSymbol` — the Sema constructs `FuncInSymbol` /
/// `FuncOutSymbol` for those instead.
PortDirection mapPortDir(ast::PortDecl::Direction d) noexcept {
  switch (d) {
  case ast::PortDecl::Direction::Input:
    return PortDirection::Input;
  case ast::PortDecl::Direction::Output:
    return PortDirection::Output;
  case ast::PortDecl::Direction::Inout:
    return PortDirection::Inout;
  case ast::PortDecl::Direction::FuncIn:
  case ast::PortDecl::Direction::FuncOut:
  case ast::PortDecl::Direction::FuncSelf:
  case ast::PortDecl::Direction::Wire:
    // Unused — caller handles control terminals + wire-class
    // declare-block items separately.
    return PortDirection::Input;
  }
  return PortDirection::Input;
}

// =====================================================================
// The pass — single top-down recursive walker
// =====================================================================

class Walker {
public:
  Walker(SymbolTable &table, TypeSystem &types, DiagnosticEngine &diag,
         ResolutionMap &resolutionMap)
      : table_(table), types_(types), diag_(diag), rmap_(resolutionMap) {}

  void runUnit(const ast::CompilationUnit &cu);

private:
  SymbolTable &table_;
  TypeSystem &types_;
  DiagnosticEngine &diag_;
  ResolutionMap &rmap_;

  /// Names that have already been reported as unresolved — per
  /// `sema-stability.contract.md` Invariant 6.
  llvm::DenseSet<llvm::StringRef> reportedUnresolved_;

  /// Pre-pass index of `declare <name> { ... }` blocks at compilation-
  /// unit scope, keyed by `<name>`. Lets the corresponding
  /// `module <name> { ... }` body re-import the declare's ports +
  /// header-params into its Module scope so references in the
  /// module body resolve cleanly. Per the design doctrine
  /// "Ref §1 — declare/module pairing": the declare names the
  /// external interface, the module supplies the body; the names
  /// declared in the declare are visible in the module body.
  llvm::StringMap<const ast::DeclareBlock *> declareByName_;

  /// Top-down width-inference context. Outermost `TransferStmt::lhs`
  /// width sits at the bottom; the topmost element is consulted by
  /// inherent-width-less Expr forms.
  std::vector<uint64_t> contextWidth_;

  // ---------- Top-level dispatch on Decl/Stmt/Expr base ----------
  void visitDecl(const ast::Decl &d);
  void visitStmt(const ast::Stmt &s);
  void visitExpr(const ast::Expr &e);

  // ---------- Decl handlers ----------
  void declTopLevelParam(const ast::TopLevelParamDecl &n);
  void declStruct(const ast::StructDecl &n);
  void declDeclareBlock(const ast::DeclareBlock &n);
  void declModuleBlock(const ast::ModuleBlock &n);
  void declPort(const ast::PortDecl &n);
  void declReg(const ast::RegDecl &n);
  void declWire(const ast::WireDecl &n);
  void declVariable(const ast::VariableDecl &n);
  void declInteger(const ast::IntegerDecl &n);
  void declMem(const ast::MemDecl &n);
  void declFuncSelf(const ast::FuncSelfDecl &n);
  void declProcName(const ast::ProcNameDecl &n);
  void declStateName(const ast::StateNameDecl &n);
  void declFirstState(const ast::FirstStateDecl &n);
  void declSubmodule(const ast::SubmoduleDecl &n);
  void declStructInst(const ast::StructInstDecl &n);
  void declFuncDefn(const ast::FuncDefn &n);
  void declProcDefn(const ast::ProcDefn &n);
  void declStateDefn(const ast::StateDefn &n);

  // ---------- Stmt handlers ----------
  void stmtTransfer(const ast::TransferStmt &n);
  void stmtIncDec(const ast::IncDecStmt &n);
  void stmtControlCall(const ast::ControlCallStmt &n);
  void stmtSystemTask(const ast::SystemTaskStmt &n);
  void stmtReturn(const ast::ReturnStmt &n);
  void stmtSeq(const ast::SeqBlock &n);
  void stmtParallel(const ast::ParallelBlock &n);
  void stmtAlt(const ast::AltBlock &n);
  void stmtAny(const ast::AnyBlock &n);
  void stmtIf(const ast::IfStmt &n);
  void stmtWhile(const ast::WhileBlock &n);
  void stmtFor(const ast::ForBlock &n);
  void stmtInitBlock(const ast::InitBlockStmt &n);
  void stmtDelayTask(const ast::DelayTaskStmt &n);
  void stmtLabeled(const ast::LabeledStmt &n);
  void stmtStructuralGenerate(const ast::StructuralGenerate &n);

  // ---------- Expr handlers (set inferredType + recurse) ----------
  void exprLiteral(const ast::LiteralExpr &n);
  void exprIdentifier(const ast::IdentifierExpr &n);
  void exprSystemVar(const ast::SystemVarExpr &n);
  void exprUnary(const ast::UnaryExpr &n);
  void exprBinary(const ast::BinaryExpr &n);
  void exprConditional(const ast::ConditionalExpr &n);
  void exprConcat(const ast::ConcatExpr &n);
  void exprRepeat(const ast::RepeatExpr &n);
  void exprSignExtend(const ast::SignExtendExpr &n);
  void exprZeroExtend(const ast::ZeroExtendExpr &n);
  void exprSlice(const ast::SliceExpr &n);
  void exprFieldAccess(const ast::FieldAccessExpr &n);
  void exprCall(const ast::CallExpr &n);
  void exprStructCast(const ast::StructCastExpr &n);
  void exprIncDec(const ast::IncDecExpr &n);

  /// Look up `name` in the symbol table. On miss, emit one
  /// "unresolved name" diagnostic per distinct name (FR-017) and
  /// return null.
  Symbol *resolveName(ast::Identifier name, SourceRange where);

  /// Record a resolution into the `ResolutionMap` (no-op for null
  /// `sym`).
  void recordResolution(const ast::Expr &e, const Symbol *sym);

  /// Set `Expr::inferredType` to the unresolved sentinel and return
  /// it. Used as the "skip me" marker for Sn walkers (FR-017).
  void markUnresolved(const ast::Expr &e);

  /// Top of the context-width stack (0 if empty).
  uint64_t topContext() const noexcept {
    return contextWidth_.empty() ? 0 : contextWidth_.back();
  }

  /// Compute the declared width of a `width` expression that
  /// accompanies a declaration (e.g., `reg q[8]`). Returns 0 if the
  /// expression is null or not a literal — for M3 stubs, complex
  /// width expressions resolve to `BitVector(1)` later.
  uint64_t declaredWidth(const ast::Expr *width) const noexcept;
};

// ---------- Walker::runUnit + dispatch ----------

void Walker::runUnit(const ast::CompilationUnit &cu) {
  // Pre-pass: index every top-level `declare <name>` block by name
  // so the matching `module <name>` body (parsed before OR after the
  // declare in source order — both are valid per the EBNF) can
  // re-import the declare's ports and header-params into its Module
  // scope. Without this pre-pass, references like `y = a & b;` in
  // a module body whose ports are declared in a sibling declare
  // block surface as "unresolved name" diagnostics.
  for (const auto &item : cu.items()) {
    if (item && item->kind() == ast::NodeKind::NK_DeclareBlock) {
      const auto &db = static_cast<const ast::DeclareBlock &>(*item);
      if (!db.name().empty()) {
        declareByName_[db.name()] = &db;
      }
    }
  }
  table_.enterScope(ScopeKind::Global);
  for (const auto &item : cu.items()) {
    if (item) {
      visitDecl(*item);
    }
  }
  table_.leaveScope();
}

void Walker::visitDecl(const ast::Decl &d) {
  switch (d.kind()) {
  case ast::NodeKind::NK_TopLevelParamDecl:
    return declTopLevelParam(static_cast<const ast::TopLevelParamDecl &>(d));
  case ast::NodeKind::NK_StructDecl:
    return declStruct(static_cast<const ast::StructDecl &>(d));
  case ast::NodeKind::NK_DeclareBlock:
    return declDeclareBlock(static_cast<const ast::DeclareBlock &>(d));
  case ast::NodeKind::NK_ModuleBlock:
    return declModuleBlock(static_cast<const ast::ModuleBlock &>(d));
  case ast::NodeKind::NK_PortDecl:
    return declPort(static_cast<const ast::PortDecl &>(d));
  case ast::NodeKind::NK_RegDecl:
    return declReg(static_cast<const ast::RegDecl &>(d));
  case ast::NodeKind::NK_WireDecl:
    return declWire(static_cast<const ast::WireDecl &>(d));
  case ast::NodeKind::NK_VariableDecl:
    return declVariable(static_cast<const ast::VariableDecl &>(d));
  case ast::NodeKind::NK_IntegerDecl:
    return declInteger(static_cast<const ast::IntegerDecl &>(d));
  case ast::NodeKind::NK_MemDecl:
    return declMem(static_cast<const ast::MemDecl &>(d));
  case ast::NodeKind::NK_FuncSelfDecl:
    return declFuncSelf(static_cast<const ast::FuncSelfDecl &>(d));
  case ast::NodeKind::NK_ProcNameDecl:
    return declProcName(static_cast<const ast::ProcNameDecl &>(d));
  case ast::NodeKind::NK_StateNameDecl:
    return declStateName(static_cast<const ast::StateNameDecl &>(d));
  case ast::NodeKind::NK_FirstStateDecl:
    return declFirstState(static_cast<const ast::FirstStateDecl &>(d));
  case ast::NodeKind::NK_SubmoduleDecl:
    return declSubmodule(static_cast<const ast::SubmoduleDecl &>(d));
  case ast::NodeKind::NK_StructInstDecl:
    return declStructInst(static_cast<const ast::StructInstDecl &>(d));
  case ast::NodeKind::NK_FuncDefn:
    return declFuncDefn(static_cast<const ast::FuncDefn &>(d));
  case ast::NodeKind::NK_ProcDefn:
    return declProcDefn(static_cast<const ast::ProcDefn &>(d));
  case ast::NodeKind::NK_StateDefn:
    return declStateDefn(static_cast<const ast::StateDefn &>(d));
  default:
    // CompilationUnit and any non-decl base reaches here only by
    // grammar mismatch; ignore silently — no diagnostic since the
    // parser already enforces shape.
    return;
  }
}

void Walker::visitStmt(const ast::Stmt &s) {
  switch (s.kind()) {
  case ast::NodeKind::NK_TransferStmt:
    return stmtTransfer(static_cast<const ast::TransferStmt &>(s));
  case ast::NodeKind::NK_IncDecStmt:
    return stmtIncDec(static_cast<const ast::IncDecStmt &>(s));
  case ast::NodeKind::NK_ControlCallStmt:
    return stmtControlCall(static_cast<const ast::ControlCallStmt &>(s));
  case ast::NodeKind::NK_BareFinishStmt:
    return; // No subtree to walk.
  case ast::NodeKind::NK_SystemTaskStmt:
    return stmtSystemTask(static_cast<const ast::SystemTaskStmt &>(s));
  case ast::NodeKind::NK_ReturnStmt:
    return stmtReturn(static_cast<const ast::ReturnStmt &>(s));
  case ast::NodeKind::NK_EmptyStmt:
    return;
  case ast::NodeKind::NK_LabeledStmt:
    return stmtLabeled(static_cast<const ast::LabeledStmt &>(s));
  case ast::NodeKind::NK_GotoStmt:
    return; // Target name is just an Identifier; resolved at Phase 4 (S25).
  case ast::NodeKind::NK_InitBlockStmt:
    return stmtInitBlock(static_cast<const ast::InitBlockStmt &>(s));
  case ast::NodeKind::NK_DelayTaskStmt:
    return stmtDelayTask(static_cast<const ast::DelayTaskStmt &>(s));
  case ast::NodeKind::NK_ParallelBlock:
    return stmtParallel(static_cast<const ast::ParallelBlock &>(s));
  case ast::NodeKind::NK_AltBlock:
    return stmtAlt(static_cast<const ast::AltBlock &>(s));
  case ast::NodeKind::NK_AnyBlock:
    return stmtAny(static_cast<const ast::AnyBlock &>(s));
  case ast::NodeKind::NK_SeqBlock:
    return stmtSeq(static_cast<const ast::SeqBlock &>(s));
  case ast::NodeKind::NK_WhileBlock:
    return stmtWhile(static_cast<const ast::WhileBlock &>(s));
  case ast::NodeKind::NK_ForBlock:
    return stmtFor(static_cast<const ast::ForBlock &>(s));
  case ast::NodeKind::NK_IfStmt:
    return stmtIf(static_cast<const ast::IfStmt &>(s));
  case ast::NodeKind::NK_StructuralGenerate:
    return stmtStructuralGenerate(
        static_cast<const ast::StructuralGenerate &>(s));
  default:
    return;
  }
}

void Walker::visitExpr(const ast::Expr &e) {
  switch (e.kind()) {
  case ast::NodeKind::NK_LiteralExpr:
    return exprLiteral(static_cast<const ast::LiteralExpr &>(e));
  case ast::NodeKind::NK_IdentifierExpr:
    return exprIdentifier(static_cast<const ast::IdentifierExpr &>(e));
  case ast::NodeKind::NK_SystemVarExpr:
    return exprSystemVar(static_cast<const ast::SystemVarExpr &>(e));
  case ast::NodeKind::NK_UnaryExpr:
    return exprUnary(static_cast<const ast::UnaryExpr &>(e));
  case ast::NodeKind::NK_BinaryExpr:
    return exprBinary(static_cast<const ast::BinaryExpr &>(e));
  case ast::NodeKind::NK_ConditionalExpr:
    return exprConditional(static_cast<const ast::ConditionalExpr &>(e));
  case ast::NodeKind::NK_ConcatExpr:
    return exprConcat(static_cast<const ast::ConcatExpr &>(e));
  case ast::NodeKind::NK_RepeatExpr:
    return exprRepeat(static_cast<const ast::RepeatExpr &>(e));
  case ast::NodeKind::NK_SignExtendExpr:
    return exprSignExtend(static_cast<const ast::SignExtendExpr &>(e));
  case ast::NodeKind::NK_ZeroExtendExpr:
    return exprZeroExtend(static_cast<const ast::ZeroExtendExpr &>(e));
  case ast::NodeKind::NK_SliceExpr:
    return exprSlice(static_cast<const ast::SliceExpr &>(e));
  case ast::NodeKind::NK_FieldAccessExpr:
    return exprFieldAccess(static_cast<const ast::FieldAccessExpr &>(e));
  case ast::NodeKind::NK_CallExpr:
    return exprCall(static_cast<const ast::CallExpr &>(e));
  case ast::NodeKind::NK_StructCastExpr:
    return exprStructCast(static_cast<const ast::StructCastExpr &>(e));
  case ast::NodeKind::NK_IncDecExpr:
    return exprIncDec(static_cast<const ast::IncDecExpr &>(e));
  default:
    // Unknown Expr kind — tag unresolved to be safe.
    markUnresolved(e);
    return;
  }
}

// ---------- Helpers ----------

uint64_t Walker::declaredWidth(const ast::Expr *width) const noexcept {
  if (!width) {
    return 0;
  }
  if (width->kind() == ast::NodeKind::NK_LiteralExpr) {
    const auto &lit = static_cast<const ast::LiteralExpr &>(*width);
    if (lit.litKind() == ast::LiteralExpr::Lit::Decimal) {
      return parseDecimal(lit.spelling());
    }
    if (lit.litKind() == ast::LiteralExpr::Lit::Hex) {
      return parseHex(lit.spelling());
    }
  }
  return 0;
}

Symbol *Walker::resolveName(ast::Identifier name, SourceRange where) {
  Symbol *sym = table_.lookup(name);
  if (sym) {
    return sym;
  }
  if (reportedUnresolved_.insert(name).second) {
    std::string msg = "unresolved name '";
    msg += name.str();
    msg += "'";
    diag_.report(Severity::Error, where.begin(), std::move(msg));
  }
  return nullptr;
}

void Walker::recordResolution(const ast::Expr &e, const Symbol *sym) {
  if (sym) {
    rmap_.exprToSymbol.insert({&e, sym});
  }
}

void Walker::markUnresolved(const ast::Expr &e) {
  // const_cast is the documented mechanism per Expr.h: setInferredType
  // is intentionally non-const. The visitor's `const Expr&` parameter
  // is the M2 visitor convention; M3 mutates the `inferredType_` slot
  // through this explicit cast.
  const_cast<ast::Expr &>(e).setInferredType(types_.unresolved());
}

// ---------- Decl handlers ----------

void Walker::declTopLevelParam(const ast::TopLevelParamDecl &n) {
  // Treated as an integer-shaped declaration in the global scope.
  bool ok = table_.declare(std::make_unique<IntegerSymbol>(n.name(), n.loc()));
  if (!ok) {
    std::string msg = "duplicate declaration of '";
    msg += n.name().str();
    msg += "'";
    diag_.report(Severity::Error, n.loc().begin(), std::move(msg));
  }
  if (n.init()) {
    visitExpr(*n.init());
  }
}

void Walker::declStruct(const ast::StructDecl &n) {
  auto sym = std::make_unique<StructTypeSymbol>(n.name(), n.loc());
  StructTypeSymbol *raw = sym.get();
  bool ok = table_.declare(std::move(sym));
  if (!ok) {
    std::string msg = "duplicate declaration of '";
    msg += n.name().str();
    msg += "'";
    diag_.report(Severity::Error, n.loc().begin(), std::move(msg));
    return;
  }
  // Walk member width expressions for inference.
  for (const auto &m : n.members()) {
    if (m.width) {
      visitExpr(*m.width);
    }
  }
  // M3 stub: populate fields with their declared widths so later
  // S18 walker (Phase 4) can refine ordering. Phase 4 will replace
  // this body with MSB-first packing.
  std::vector<FieldInfo> fields;
  uint64_t total = 0;
  for (const auto &m : n.members()) {
    uint64_t w = declaredWidth(m.width.get());
    if (w == 0) {
      w = 1;
    }
    fields.push_back(FieldInfo{m.name, w, total});
    total += w;
  }
  raw->setFields(std::move(fields), total);
  raw->setType(types_.structType(n.name(), raw->fields().vec(), total));
}

void Walker::declDeclareBlock(const ast::DeclareBlock &n) {
  // The DeclareBlock itself names an interface/template — declare
  // it in the current (Global) scope so submodule references can
  // find the declare-block by name. We use a SubmoduleSymbol-like
  // surface — actually, the design says DeclareBlocks are first-
  // class entities resolvable via a future SubmoduleDecl. For M3
  // we just open a Declare scope and recurse into ports.
  table_.enterScope(ScopeKind::Declare);
  for (const auto &param : n.headerParams()) {
    if (param) {
      visitDecl(*param);
    }
  }
  for (const auto &port : n.ports()) {
    if (port) {
      visitDecl(*port);
    }
  }
  table_.leaveScope();
}

void Walker::declModuleBlock(const ast::ModuleBlock &n) {
  table_.enterScope(ScopeKind::Module);
  // Re-declare the matching `declare <name>` block's ports and
  // header-params into this Module scope so module-body references
  // resolve. The declare itself was already walked at top-level
  // (its own Declare scope owns the canonical Symbol*); here we
  // synthesize a parallel set of Symbols in the Module scope for
  // resolution purposes. The two sets co-exist; downstream Sn
  // walkers consult whichever is in scope at the use site.
  if (!n.name().empty()) {
    auto it = declareByName_.find(n.name());
    if (it != declareByName_.end() && it->second != nullptr) {
      const ast::DeclareBlock &db = *it->second;
      for (const auto &param : db.headerParams()) {
        if (param) {
          visitDecl(*param);
        }
      }
      for (const auto &port : db.ports()) {
        if (port) {
          visitDecl(*port);
        }
      }
    }
  }
  for (const auto &i : n.internals()) {
    if (i) {
      visitDecl(*i);
    }
  }
  // Funcs and procs declared in the same Module scope. Their
  // bodies open further nested scopes.
  for (const auto &f : n.funcs()) {
    if (f) {
      visitDecl(*f);
    }
  }
  for (const auto &p : n.procs()) {
    if (p) {
      visitDecl(*p);
    }
  }
  for (const auto &a : n.actions()) {
    if (a) {
      visitStmt(*a);
    }
  }
  table_.leaveScope();
}

void Walker::declPort(const ast::PortDecl &n) {
  std::unique_ptr<Symbol> sym;
  switch (n.direction()) {
  case ast::PortDecl::Direction::Input:
  case ast::PortDecl::Direction::Output:
  case ast::PortDecl::Direction::Inout:
    sym = std::make_unique<PortSymbol>(n.name(), n.loc(),
                                       mapPortDir(n.direction()));
    break;
  case ast::PortDecl::Direction::FuncIn:
    sym = std::make_unique<FuncInSymbol>(n.name(), n.loc());
    break;
  case ast::PortDecl::Direction::FuncOut:
    sym = std::make_unique<FuncOutSymbol>(n.name(), n.loc());
    break;
  case ast::PortDecl::Direction::FuncSelf:
    sym = std::make_unique<FuncSelfSymbol>(n.name(), n.loc());
    break;
  case ast::PortDecl::Direction::Wire:
    // Wire-class declare-block terminal — model as a WireSymbol so
    // S4's func_self dummy-arg check can identify it. Width
    // defaults to bit if not set.
    sym = std::make_unique<WireSymbol>(n.name(), n.loc());
    break;
  }
  Symbol *raw = sym.get();
  if (!table_.declare(std::move(sym))) {
    std::string msg = "duplicate declaration of '";
    msg += n.name().str();
    msg += "'";
    diag_.report(Severity::Error, n.loc().begin(), std::move(msg));
    return;
  }
  // Set declared type from the width expression.
  uint64_t w = declaredWidth(n.width());
  if (w == 0) {
    raw->setType(types_.bit());
  } else {
    raw->setType(types_.bitVector(w));
  }
  if (n.width()) {
    visitExpr(*n.width());
  }
}

void Walker::declReg(const ast::RegDecl &n) {
  auto sym = std::make_unique<RegSymbol>(n.name(), n.loc());
  RegSymbol *raw = sym.get();
  if (!table_.declare(std::move(sym))) {
    std::string msg = "duplicate declaration of '";
    msg += n.name().str();
    msg += "'";
    diag_.report(Severity::Error, n.loc().begin(), std::move(msg));
    return;
  }
  uint64_t w = declaredWidth(n.width());
  if (w == 0) {
    // S23: omitted width with init → BitVector(1); else default to
    // BitVector(1) too as a Phase 3 stub.
    raw->setType(types_.bitVector(1));
  } else {
    raw->setType(types_.bitVector(w));
  }
  if (n.width()) {
    visitExpr(*n.width());
  }
  if (n.init()) {
    visitExpr(*n.init());
  }
}

void Walker::declWire(const ast::WireDecl &n) {
  auto sym = std::make_unique<WireSymbol>(n.name(), n.loc());
  WireSymbol *raw = sym.get();
  if (!table_.declare(std::move(sym))) {
    std::string msg = "duplicate declaration of '";
    msg += n.name().str();
    msg += "'";
    diag_.report(Severity::Error, n.loc().begin(), std::move(msg));
    return;
  }
  uint64_t w = declaredWidth(n.width());
  raw->setType(types_.bitVector(w == 0 ? 1 : w));
  if (n.width()) {
    visitExpr(*n.width());
  }
}

void Walker::declVariable(const ast::VariableDecl &n) {
  auto sym = std::make_unique<VariableSymbol>(n.name(), n.loc());
  VariableSymbol *raw = sym.get();
  if (!table_.declare(std::move(sym))) {
    std::string msg = "duplicate declaration of '";
    msg += n.name().str();
    msg += "'";
    diag_.report(Severity::Error, n.loc().begin(), std::move(msg));
    return;
  }
  uint64_t w = declaredWidth(n.width());
  raw->setType(types_.bitVector(w == 0 ? 1 : w));
  if (n.width()) {
    visitExpr(*n.width());
  }
}

void Walker::declInteger(const ast::IntegerDecl &n) {
  auto sym = std::make_unique<IntegerSymbol>(n.name(), n.loc());
  IntegerSymbol *raw = sym.get();
  if (!table_.declare(std::move(sym))) {
    std::string msg = "duplicate declaration of '";
    msg += n.name().str();
    msg += "'";
    diag_.report(Severity::Error, n.loc().begin(), std::move(msg));
    return;
  }
  // Integer is host-int sized — use BitVector(64) as a stable type.
  raw->setType(types_.bitVector(64));
}

void Walker::declMem(const ast::MemDecl &n) {
  auto sym = std::make_unique<MemSymbol>(n.name(), n.loc());
  MemSymbol *raw = sym.get();
  if (!table_.declare(std::move(sym))) {
    std::string msg = "duplicate declaration of '";
    msg += n.name().str();
    msg += "'";
    diag_.report(Severity::Error, n.loc().begin(), std::move(msg));
    return;
  }
  uint64_t depth = declaredWidth(n.depth());
  uint64_t w = declaredWidth(n.width());
  if (depth == 0) {
    depth = 1;
  }
  TypeRef element = types_.bitVector(w == 0 ? 1 : w);
  raw->setType(types_.memory(depth, element));
  if (n.depth()) {
    visitExpr(*n.depth());
  }
  if (n.width()) {
    visitExpr(*n.width());
  }
  for (const auto &v : n.init()) {
    if (v) {
      visitExpr(*v);
    }
  }
}

void Walker::declFuncSelf(const ast::FuncSelfDecl &n) {
  auto sym = std::make_unique<FuncSelfSymbol>(n.name(), n.loc());
  FuncSelfSymbol *raw = sym.get();
  if (!table_.declare(std::move(sym))) {
    std::string msg = "duplicate declaration of '";
    msg += n.name().str();
    msg += "'";
    diag_.report(Severity::Error, n.loc().begin(), std::move(msg));
    return;
  }
  raw->setType(types_.bit()); // control-shaped 1-bit tap
}

void Walker::declProcName(const ast::ProcNameDecl &n) {
  auto sym = std::make_unique<ProcSymbol>(n.name(), n.loc());
  ProcSymbol *raw = sym.get();
  if (!table_.declare(std::move(sym))) {
    std::string msg = "duplicate declaration of '";
    msg += n.name().str();
    msg += "'";
    diag_.report(Severity::Error, n.loc().begin(), std::move(msg));
    return;
  }
  raw->setType(types_.bit());
  // The reg-args are textual identifiers; they refer to RegSymbols
  // declared elsewhere in the module — Phase 4 S6 walker validates.
}

void Walker::declStateName(const ast::StateNameDecl &n) {
  for (auto name : n.names()) {
    auto sym = std::make_unique<StateSymbol>(name, n.loc());
    StateSymbol *raw = sym.get();
    // Lift the state_name into the nearest enclosing Module scope
    // (rather than the proc's local scope) so references from
    // sibling func/proc/module-action bodies resolve. The S11
    // checker then enforces that the use site's enclosing proc is
    // the declaring proc — references from elsewhere fire S11
    // instead of cascading into "unresolved name" diagnostics.
    if (!table_.declareInScope(ScopeKind::Module, std::move(sym))) {
      std::string msg = "duplicate declaration of '";
      msg += name.str();
      msg += "'";
      diag_.report(Severity::Error, n.loc().begin(), std::move(msg));
      continue;
    }
    raw->setType(types_.bit());
  }
}

void Walker::declFirstState(const ast::FirstStateDecl &n) {
  // Just a reference; Phase 4 S28 walker validates target exists.
  (void)n;
}

void Walker::declSubmodule(const ast::SubmoduleDecl &n) {
  // Each instance becomes a SubmoduleSymbol; templateDecl is left
  // null at M3 (Phase 4 S20 / M5 lowering populates).
  for (const auto &inst : n.instances()) {
    auto sym = std::make_unique<SubmoduleSymbol>(inst.name, n.loc(), nullptr);
    SubmoduleSymbol *raw = sym.get();
    if (!table_.declare(std::move(sym))) {
      std::string msg = "duplicate declaration of '";
      msg += inst.name.str();
      msg += "'";
      diag_.report(Severity::Error, n.loc().begin(), std::move(msg));
      continue;
    }
    raw->setType(types_.bit());
    if (inst.arraySize) {
      visitExpr(*inst.arraySize);
    }
  }
  for (const auto &p : n.paramAssigns()) {
    if (p.value) {
      visitExpr(*p.value);
    }
  }
}

void Walker::declStructInst(const ast::StructInstDecl &n) {
  auto sym = std::make_unique<RegSymbol>(n.instanceName(), n.loc());
  RegSymbol *raw = sym.get();
  if (!table_.declare(std::move(sym))) {
    std::string msg = "duplicate declaration of '";
    msg += n.instanceName().str();
    msg += "'";
    diag_.report(Severity::Error, n.loc().begin(), std::move(msg));
    return;
  }
  raw->setType(types_.bitVector(1));
  if (n.arraySize()) {
    visitExpr(*n.arraySize());
  }
  for (const auto &v : n.init()) {
    if (v) {
      visitExpr(*v);
    }
  }
}

void Walker::declFuncDefn(const ast::FuncDefn &n) {
  // Single-part name → declare a FuncInSymbol if not already present.
  // Multi-part name (inst.func) → look up `inst` (Submodule) — Phase 4.
  if (n.name().parts.size() == 1) {
    Symbol *existing = table_.lookup(n.name().parts.front());
    if (!existing) {
      auto sym =
          std::make_unique<FuncInSymbol>(n.name().parts.front(), n.loc());
      FuncInSymbol *raw = sym.get();
      table_.declare(std::move(sym));
      raw->setType(types_.bit());
    }
  }
  table_.enterScope(ScopeKind::Function);
  if (n.body()) {
    visitStmt(*n.body());
  }
  table_.leaveScope();
}

void Walker::declProcDefn(const ast::ProcDefn &n) {
  Symbol *existing = table_.lookup(n.name());
  if (!existing) {
    auto sym = std::make_unique<ProcSymbol>(n.name(), n.loc());
    ProcSymbol *raw = sym.get();
    table_.declare(std::move(sym));
    raw->setType(types_.bit());
  }
  table_.enterScope(ScopeKind::Proc);
  if (n.body()) {
    visitStmt(*n.body());
  }
  table_.leaveScope();
}

void Walker::declStateDefn(const ast::StateDefn &n) {
  // The state name should already be declared via StateNameDecl.
  // We open a new SeqOrParallel scope for the body so locals are
  // contained.
  table_.enterScope(ScopeKind::SeqOrParallel);
  if (n.body()) {
    visitStmt(*n.body());
  }
  table_.leaveScope();
}

// ---------- Stmt handlers ----------

void Walker::stmtTransfer(const ast::TransferStmt &n) {
  // Push the LHS-derived width as the context for the RHS subtree.
  uint64_t lhsWidth = 0;
  if (n.lhs()) {
    // Walk LHS first to populate inferredType.
    visitExpr(*n.lhs());
    const Type *t = n.lhs()->inferredType();
    if (t && t->kind() == TypeKind::BitVector) {
      lhsWidth = static_cast<const BitVectorType *>(t)->width();
    }
  }
  contextWidth_.push_back(lhsWidth);
  if (n.rhs()) {
    visitExpr(*n.rhs());
  }
  contextWidth_.pop_back();
}

void Walker::stmtIncDec(const ast::IncDecStmt &n) {
  if (n.target()) {
    visitExpr(*n.target());
  }
}

void Walker::stmtControlCall(const ast::ControlCallStmt &n) {
  // Resolve the head identifier; tail is recognized as a built-in
  // proc-method (`finish`, `invoke`) per S21 — but Phase 3 just
  // attempts head lookup. Multi-part ScopedName fails silently
  // since multi-part is the proc-method path.
  const auto &target = n.target();
  if (!target.parts.empty() && target.parts.size() == 1) {
    (void)resolveName(target.parts.front(), n.loc());
  }
  for (const auto &arg : n.args()) {
    if (arg) {
      visitExpr(*arg);
    }
  }
}

void Walker::stmtSystemTask(const ast::SystemTaskStmt &n) {
  for (const auto &arg : n.args()) {
    if (arg) {
      visitExpr(*arg);
    }
  }
}

void Walker::stmtReturn(const ast::ReturnStmt &n) {
  if (n.value()) {
    visitExpr(*n.value());
  }
}

void Walker::stmtSeq(const ast::SeqBlock &n) {
  table_.enterScope(ScopeKind::SeqOrParallel);
  for (const auto &item : n.items()) {
    if (item) {
      visitStmt(*item);
    }
  }
  table_.leaveScope();
}

void Walker::stmtParallel(const ast::ParallelBlock &n) {
  table_.enterScope(ScopeKind::SeqOrParallel);
  for (const auto &item : n.items()) {
    if (item) {
      visitStmt(*item);
    }
  }
  table_.leaveScope();
}

void Walker::stmtAlt(const ast::AltBlock &n) {
  table_.enterScope(ScopeKind::SeqOrParallel);
  for (const auto &c : n.cases()) {
    if (c.cond) {
      visitExpr(*c.cond);
    }
    if (c.body) {
      visitStmt(*c.body);
    }
  }
  if (n.elseCase()) {
    visitStmt(*n.elseCase());
  }
  table_.leaveScope();
}

void Walker::stmtAny(const ast::AnyBlock &n) {
  table_.enterScope(ScopeKind::SeqOrParallel);
  for (const auto &c : n.cases()) {
    if (c.cond) {
      visitExpr(*c.cond);
    }
    if (c.body) {
      visitStmt(*c.body);
    }
  }
  if (n.elseCase()) {
    visitStmt(*n.elseCase());
  }
  table_.leaveScope();
}

void Walker::stmtIf(const ast::IfStmt &n) {
  if (n.cond()) {
    visitExpr(*n.cond());
  }
  if (n.thenBr()) {
    visitStmt(*n.thenBr());
  }
  if (n.elseBr()) {
    visitStmt(*n.elseBr());
  }
}

void Walker::stmtWhile(const ast::WhileBlock &n) {
  table_.enterScope(ScopeKind::SeqOrParallel);
  if (n.cond()) {
    visitExpr(*n.cond());
  }
  for (const auto &item : n.items()) {
    if (item) {
      visitStmt(*item);
    }
  }
  table_.leaveScope();
}

void Walker::stmtFor(const ast::ForBlock &n) {
  table_.enterScope(ScopeKind::SeqOrParallel);
  if (n.form().init) {
    visitStmt(*n.form().init);
  }
  if (n.form().cond) {
    visitExpr(*n.form().cond);
  }
  if (n.form().step) {
    visitStmt(*n.form().step);
  }
  for (const auto &item : n.items()) {
    if (item) {
      visitStmt(*item);
    }
  }
  table_.leaveScope();
}

void Walker::stmtInitBlock(const ast::InitBlockStmt &n) {
  table_.enterScope(ScopeKind::SeqOrParallel);
  for (const auto &item : n.items()) {
    if (item) {
      visitStmt(*item);
    }
  }
  table_.leaveScope();
}

void Walker::stmtDelayTask(const ast::DelayTaskStmt &n) {
  if (n.count()) {
    visitExpr(*n.count());
  }
}

void Walker::stmtLabeled(const ast::LabeledStmt &n) {
  if (n.body()) {
    visitStmt(*n.body());
  }
}

void Walker::stmtStructuralGenerate(const ast::StructuralGenerate &n) {
  table_.enterScope(ScopeKind::SeqOrParallel);
  if (n.cond()) {
    visitExpr(*n.cond());
  }
  if (n.step()) {
    visitExpr(*n.step());
  }
  if (n.body()) {
    visitStmt(*n.body());
  }
  table_.leaveScope();
}

// ---------- Expr handlers ----------

void Walker::exprLiteral(const ast::LiteralExpr &n) {
  TypeRef t = literalType(n, types_, topContext());
  const_cast<ast::LiteralExpr &>(n).setInferredType(t);
}

void Walker::exprIdentifier(const ast::IdentifierExpr &n) {
  const auto &sn = n.name();
  if (sn.parts.empty()) {
    markUnresolved(n);
    return;
  }
  if (sn.parts.size() == 1) {
    Symbol *sym = resolveName(sn.parts.front(), n.loc());
    if (!sym) {
      markUnresolved(n);
      return;
    }
    recordResolution(n, sym);
    TypeRef t = sym->type();
    if (!t) {
      t = types_.unresolved();
    }
    const_cast<ast::IdentifierExpr &>(n).setInferredType(t);
    return;
  }
  // Multi-part — Phase 3 strategy: resolve the HEAD only. The tail
  // is a field / method / port reference whose validation is Phase
  // 4 (S18 for struct fields; S21 for proc methods; submodule port
  // walks via SubmoduleSymbol::templateDecl for SUB.port). For
  // Phase 3 MVP, we record the head's Symbol* in the resolution map
  // (so the printer's `→ decl@` suffix works) and assign the
  // identifier expression a placeholder type that downstream Sn
  // walkers can refine.
  Symbol *head = resolveName(sn.parts.front(), n.loc());
  if (!head) {
    markUnresolved(n);
    return;
  }
  recordResolution(n, head);
  // Use the head's type as a stub for the dotted reference; Phase 4
  // refines (struct field walker reads StructTypeSymbol::fields()
  // for the tail, etc.).
  TypeRef t = head->type();
  if (!t) {
    t = types_.bitVector(1);
  }
  const_cast<ast::IdentifierExpr &>(n).setInferredType(t);
}

void Walker::exprSystemVar(const ast::SystemVarExpr &n) {
  // _random / _time are simulation-only host-int values; treat as
  // BitVector(64).
  const_cast<ast::SystemVarExpr &>(n).setInferredType(types_.bitVector(64));
}

void Walker::exprUnary(const ast::UnaryExpr &n) {
  if (n.sub()) {
    visitExpr(*n.sub());
  }
  TypeRef t = n.sub() ? n.sub()->inferredType() : nullptr;
  // Reduction operators (& | ^) produce a 1-bit result.
  switch (n.op()) {
  case ast::UnaryExpr::Op::ReduceAnd:
  case ast::UnaryExpr::Op::ReduceOr:
  case ast::UnaryExpr::Op::ReduceXor:
  case ast::UnaryExpr::Op::LogicalNot:
    t = types_.bitVector(1);
    break;
  default:
    if (!t) {
      t = topContext() != 0 ? types_.bitVector(topContext())
                            : types_.bitVector(1);
    }
    break;
  }
  const_cast<ast::UnaryExpr &>(n).setInferredType(t);
}

void Walker::exprBinary(const ast::BinaryExpr &n) {
  if (n.lhs()) {
    visitExpr(*n.lhs());
  }
  if (n.rhs()) {
    visitExpr(*n.rhs());
  }
  TypeRef tl = n.lhs() ? n.lhs()->inferredType() : nullptr;
  TypeRef tr = n.rhs() ? n.rhs()->inferredType() : nullptr;
  TypeRef result = tl ? tl : tr;
  // Relational / logical → 1-bit.
  switch (n.op()) {
  case ast::BinaryExpr::Op::Equal:
  case ast::BinaryExpr::Op::NotEqual:
  case ast::BinaryExpr::Op::Less:
  case ast::BinaryExpr::Op::LessEqual:
  case ast::BinaryExpr::Op::Greater:
  case ast::BinaryExpr::Op::GreaterEqual:
  case ast::BinaryExpr::Op::LogicalAnd:
  case ast::BinaryExpr::Op::LogicalOr:
    result = types_.bitVector(1);
    break;
  default:
    break;
  }
  if (!result) {
    result = topContext() != 0 ? types_.bitVector(topContext())
                               : types_.bitVector(1);
  }
  // Propagate Unresolved if either operand was unresolved.
  if ((tl && tl->kind() == TypeKind::Unresolved) ||
      (tr && tr->kind() == TypeKind::Unresolved)) {
    result = types_.unresolved();
  }
  const_cast<ast::BinaryExpr &>(n).setInferredType(result);
}

void Walker::exprConditional(const ast::ConditionalExpr &n) {
  if (n.cond()) {
    visitExpr(*n.cond());
  }
  if (n.thenE()) {
    visitExpr(*n.thenE());
  }
  if (n.elseE()) {
    visitExpr(*n.elseE());
  }
  TypeRef t = n.thenE() ? n.thenE()->inferredType() : nullptr;
  if (!t) {
    t = n.elseE() ? n.elseE()->inferredType() : nullptr;
  }
  if (!t) {
    t = topContext() != 0 ? types_.bitVector(topContext())
                          : types_.bitVector(1);
  }
  const_cast<ast::ConditionalExpr &>(n).setInferredType(t);
}

void Walker::exprConcat(const ast::ConcatExpr &n) {
  uint64_t total = 0;
  bool any_unresolved = false;
  for (const auto &p : n.parts()) {
    if (p) {
      visitExpr(*p);
      const Type *t = p->inferredType();
      if (t && t->kind() == TypeKind::BitVector) {
        total += static_cast<const BitVectorType *>(t)->width();
      } else if (t && t->kind() == TypeKind::Unresolved) {
        any_unresolved = true;
      }
    }
  }
  if (any_unresolved) {
    const_cast<ast::ConcatExpr &>(n).setInferredType(types_.unresolved());
    return;
  }
  if (total == 0) {
    total = 1;
  }
  const_cast<ast::ConcatExpr &>(n).setInferredType(types_.bitVector(total));
}

void Walker::exprRepeat(const ast::RepeatExpr &n) {
  uint64_t count = 0;
  if (n.count()) {
    visitExpr(*n.count());
    if (n.count()->kind() == ast::NodeKind::NK_LiteralExpr) {
      const auto &lit = static_cast<const ast::LiteralExpr &>(*n.count());
      if (lit.litKind() == ast::LiteralExpr::Lit::Decimal) {
        count = parseDecimal(lit.spelling());
      }
    }
  }
  uint64_t bw = 0;
  if (n.body()) {
    visitExpr(*n.body());
    const Type *t = n.body()->inferredType();
    if (t && t->kind() == TypeKind::BitVector) {
      bw = static_cast<const BitVectorType *>(t)->width();
    } else if (t && t->kind() == TypeKind::Unresolved) {
      const_cast<ast::RepeatExpr &>(n).setInferredType(types_.unresolved());
      return;
    }
  }
  uint64_t total = count * bw;
  if (total == 0) {
    total = 1;
  }
  const_cast<ast::RepeatExpr &>(n).setInferredType(types_.bitVector(total));
}

void Walker::exprSignExtend(const ast::SignExtendExpr &n) {
  uint64_t w = declaredWidth(n.width());
  if (n.width()) {
    visitExpr(*n.width());
  }
  if (n.sub()) {
    visitExpr(*n.sub());
  }
  if (w == 0) {
    w = 1;
  }
  const_cast<ast::SignExtendExpr &>(n).setInferredType(types_.bitVector(w));
}

void Walker::exprZeroExtend(const ast::ZeroExtendExpr &n) {
  uint64_t w = declaredWidth(n.width());
  if (n.width()) {
    visitExpr(*n.width());
  }
  if (n.sub()) {
    visitExpr(*n.sub());
  }
  if (w == 0) {
    w = 1;
  }
  const_cast<ast::ZeroExtendExpr &>(n).setInferredType(types_.bitVector(w));
}

void Walker::exprSlice(const ast::SliceExpr &n) {
  if (n.sub()) {
    visitExpr(*n.sub());
  }
  uint64_t hi = 0;
  uint64_t lo = 0;
  if (n.hi()) {
    visitExpr(*n.hi());
    if (n.hi()->kind() == ast::NodeKind::NK_LiteralExpr) {
      const auto &lit = static_cast<const ast::LiteralExpr &>(*n.hi());
      if (lit.litKind() == ast::LiteralExpr::Lit::Decimal) {
        hi = parseDecimal(lit.spelling());
      }
    }
  }
  if (n.lo()) {
    visitExpr(*n.lo());
    if (n.lo()->kind() == ast::NodeKind::NK_LiteralExpr) {
      const auto &lit = static_cast<const ast::LiteralExpr &>(*n.lo());
      if (lit.litKind() == ast::LiteralExpr::Lit::Decimal) {
        lo = parseDecimal(lit.spelling());
      }
    }
  }
  uint64_t w = 0;
  if (n.lo()) {
    if (hi >= lo) {
      w = (hi - lo) + 1U;
    }
  } else {
    w = 1; // single-bit slice `x[hi]`.
  }
  if (w == 0) {
    w = 1;
  }
  const_cast<ast::SliceExpr &>(n).setInferredType(types_.bitVector(w));
}

void Walker::exprFieldAccess(const ast::FieldAccessExpr &n) {
  if (n.obj()) {
    visitExpr(*n.obj());
  }
  // Phase 3 stub: type is the parent's type if BitVector, else
  // BitVector(1). Phase 4 S18 walker refines using
  // StructTypeSymbol::fields().
  TypeRef t = n.obj() ? n.obj()->inferredType() : nullptr;
  if (!t) {
    t = types_.bitVector(1);
  } else if (t->kind() == TypeKind::Unresolved) {
    t = types_.unresolved();
  } else {
    // Default: treat field as 1-bit until S18 lands.
    t = types_.bitVector(1);
  }
  const_cast<ast::FieldAccessExpr &>(n).setInferredType(t);
}

void Walker::exprCall(const ast::CallExpr &n) {
  const auto &target = n.target();
  if (!target.parts.empty()) {
    if (target.parts.size() == 1) {
      Symbol *sym = resolveName(target.parts.front(), n.loc());
      if (sym) {
        recordResolution(n, sym);
      }
    } else {
      Symbol *sym = table_.lookupScoped(target);
      if (sym) {
        recordResolution(n, sym);
      }
    }
  }
  for (const auto &arg : n.args()) {
    if (arg) {
      visitExpr(*arg);
    }
  }
  // Function-call result: 1-bit by default; refined when the
  // resolved symbol's type is known.
  TypeRef t = types_.bitVector(1);
  const_cast<ast::CallExpr &>(n).setInferredType(t);
}

void Walker::exprStructCast(const ast::StructCastExpr &n) {
  if (n.sub()) {
    visitExpr(*n.sub());
  }
  Symbol *sym = table_.lookup(n.typeName());
  if (sym && sym->kind() == SymbolKind::SK_StructType) {
    const auto *sts = static_cast<const StructTypeSymbol *>(sym);
    const_cast<ast::StructCastExpr &>(n).setInferredType(
        types_.bitVector(sts->totalWidth() == 0 ? 1 : sts->totalWidth()));
    return;
  }
  // Unknown struct → 1-bit.
  const_cast<ast::StructCastExpr &>(n).setInferredType(types_.bitVector(1));
}

void Walker::exprIncDec(const ast::IncDecExpr &n) {
  if (n.target()) {
    visitExpr(*n.target());
  }
  TypeRef t = n.target() ? n.target()->inferredType() : nullptr;
  if (!t) {
    t = types_.bitVector(1);
  }
  const_cast<ast::IncDecExpr &>(n).setInferredType(t);
}

} // namespace

// =====================================================================
// Public entry points
// =====================================================================

ResolutionMap runResolutionPassImpl(const ast::CompilationUnit &unit,
                                    SymbolTable &symbols, TypeSystem &types,
                                    DiagnosticEngine &diag) {
  ResolutionMap rmap;
  Walker walker(symbols, types, diag, rmap);
  walker.runUnit(unit);
  return rmap;
}

// TLS hook for the printer. Single-threaded compiler — no contention.
namespace {
thread_local const ResolutionMap *g_currentMap = nullptr;
} // namespace

const ResolutionMap *currentResolutionMap() noexcept {
  return g_currentMap;
}

void setCurrentResolutionMap(const ResolutionMap *map) noexcept {
  g_currentMap = map;
}

// Public wrapper consumed by `lib/Driver/EmitAST.cpp` (forward-
// declared there to avoid pulling the private `ResolutionPass.h`
// into the driver layer). Returns the resolved declaration's
// `SourceRange` for `e` or an invalid range if unresolved.
SourceRange lookupDeclLoc(const ast::Expr *e) noexcept {
  if (!e) {
    return SourceRange{};
  }
  const ResolutionMap *map = currentResolutionMap();
  if (!map) {
    return SourceRange{};
  }
  auto it = map->exprToSymbol.find(e);
  if (it == map->exprToSymbol.end()) {
    return SourceRange{};
  }
  if (!it->second) {
    return SourceRange{};
  }
  return it->second->declLoc();
}

} // namespace nsl::sema
