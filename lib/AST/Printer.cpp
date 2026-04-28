// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// lib/AST/Printer.cpp — text-only S-expression-style AST printer
// implementing `nsl::ast::print()` from `include/nsl/AST/Printer.h`.
//
// Format: per `contracts/nslc-emit-ast.contract.md` §"Stdout schema".
// Each node opens with `(NodeKind  loc=<path:line:col-line:col>` plus
// kind-specific `field=value` pairs whitespace-separated; child nodes
// are recursively printed at indent+2 with `\n` between siblings; the
// last child's line absorbs the parent's closing `)` (matching the
// contract example).
//
// Determinism mechanics (Invariants 2/3/4 in
// `contracts/ast-stability.contract.md`):
//   - Output is a pure function of (AST, SourceManager). No
//     `std::unordered_*` iteration; no pointer prints; no environment
//     reads beyond what `SourceManager::resolveVirtual()` already
//     supplies.
//   - Every collection iterated below is `std::vector` (insertion-
//     ordered) — invariants checked at code-review time per Invariant
//     4 enforcement note.
//   - Cross-references between AST nodes (per data-model §6) serialize
//     as `ref=path:line:col` of the target's `SourceRange::begin()`
//     (Invariant 3). At M2 the only such references are textual
//     `Identifier` strings (e.g., `goto target=foo`); the AST does not
//     yet carry resolved `ASTNode*` cross-pointers.
//
// Schema notes for M2:
//   - Aggregate sub-records (`StructMember`, `CondCase`,
//     `SubmoduleDecl::Instance`, `SubmoduleDecl::ParamAssign`,
//     `ForForm`) that bundle an identifier with one or more Exprs are
//     rendered as synthetic wrapper lines — "(<TagName>  name=<id>"
//     with any contained Exprs as children. These wrappers are not
//     `NodeKind` enumerators; the angle-bracketed tag name flags them
//     as printer-synthetic. M3+ may promote any of them to a real
//     `NodeKind` (with the goldens re-cut in the same patch per
//     Invariant 7 additivity).

#include "nsl/AST/Printer.h"

#include "nsl/AST/ASTNode.h"
#include "nsl/AST/ASTVisitor.h"
#include "nsl/AST/Expr.h"
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
#include "nsl/AST/NodeKind.h"
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
#include "nsl/Basic/SourceLocation.h"
#include "nsl/Basic/SourceManager.h"

#include "llvm/ADT/StringRef.h"
#include "llvm/Support/raw_ostream.h"

#include <cstddef>
#include <vector>

namespace nsl::ast {
namespace {

// ---------- Enum-to-string helpers (no `std::unordered_*`) ----------

llvm::StringRef toString(TopLevelParamDecl::ParamKind k) {
  switch (k) {
  case TopLevelParamDecl::ParamKind::Int:
    return "Int";
  case TopLevelParamDecl::ParamKind::Str:
    return "Str";
  }
  return "<invalid-ParamKind>";
}

llvm::StringRef toString(DeclareBlock::Modifier m) {
  switch (m) {
  case DeclareBlock::Modifier::None:
    return "None";
  case DeclareBlock::Modifier::Interface:
    return "Interface";
  case DeclareBlock::Modifier::Simulation:
    return "Simulation";
  }
  return "<invalid-Modifier>";
}

llvm::StringRef toString(PortDecl::Direction d) {
  switch (d) {
  case PortDecl::Direction::Input:
    return "Input";
  case PortDecl::Direction::Output:
    return "Output";
  case PortDecl::Direction::Inout:
    return "Inout";
  case PortDecl::Direction::FuncIn:
    return "FuncIn";
  case PortDecl::Direction::FuncOut:
    return "FuncOut";
  }
  return "<invalid-Direction>";
}

llvm::StringRef toString(StructInstDecl::StorageKind k) {
  switch (k) {
  case StructInstDecl::StorageKind::Reg:
    return "Reg";
  case StructInstDecl::StorageKind::Wire:
    return "Wire";
  }
  return "<invalid-StorageKind>";
}

llvm::StringRef toString(TransferStmt::Op op) {
  switch (op) {
  case TransferStmt::Op::WireEq:
    return "WireEq";
  case TransferStmt::Op::RegColonEq:
    return "RegColonEq";
  }
  return "<invalid-TransferOp>";
}

llvm::StringRef toString(IncDecStmt::Op op) {
  switch (op) {
  case IncDecStmt::Op::Inc:
    return "Inc";
  case IncDecStmt::Op::Dec:
    return "Dec";
  }
  return "<invalid-IncDecOp>";
}

llvm::StringRef toString(IncDecExpr::Op op) {
  switch (op) {
  case IncDecExpr::Op::Inc:
    return "Inc";
  case IncDecExpr::Op::Dec:
    return "Dec";
  }
  return "<invalid-IncDecOp>";
}

llvm::StringRef toString(LiteralExpr::Lit l) {
  switch (l) {
  case LiteralExpr::Lit::Decimal:
    return "Decimal";
  case LiteralExpr::Lit::Hex:
    return "Hex";
  case LiteralExpr::Lit::Binary:
    return "Binary";
  case LiteralExpr::Lit::Octal:
    return "Octal";
  case LiteralExpr::Lit::String:
    return "String";
  }
  return "<invalid-Lit>";
}

llvm::StringRef toString(SystemVarExpr::Var v) {
  switch (v) {
  case SystemVarExpr::Var::Random:
    return "Random";
  case SystemVarExpr::Var::Time:
    return "Time";
  }
  return "<invalid-SystemVar>";
}

llvm::StringRef toString(UnaryExpr::Op op) {
  switch (op) {
  case UnaryExpr::Op::Neg:
    return "Neg";
  case UnaryExpr::Op::Plus:
    return "Plus";
  case UnaryExpr::Op::BitNot:
    return "BitNot";
  case UnaryExpr::Op::LogicalNot:
    return "LogicalNot";
  case UnaryExpr::Op::ReduceAnd:
    return "ReduceAnd";
  case UnaryExpr::Op::ReduceOr:
    return "ReduceOr";
  case UnaryExpr::Op::ReduceXor:
    return "ReduceXor";
  }
  return "<invalid-UnaryOp>";
}

llvm::StringRef toString(BinaryExpr::Op op) {
  switch (op) {
  case BinaryExpr::Op::Add:
    return "Add";
  case BinaryExpr::Op::Sub:
    return "Sub";
  case BinaryExpr::Op::Mul:
    return "Mul";
  case BinaryExpr::Op::Div:
    return "Div";
  case BinaryExpr::Op::Mod:
    return "Mod";
  case BinaryExpr::Op::BitAnd:
    return "BitAnd";
  case BinaryExpr::Op::BitOr:
    return "BitOr";
  case BinaryExpr::Op::BitXor:
    return "BitXor";
  case BinaryExpr::Op::ShiftLeft:
    return "ShiftLeft";
  case BinaryExpr::Op::ShiftRight:
    return "ShiftRight";
  case BinaryExpr::Op::Equal:
    return "Equal";
  case BinaryExpr::Op::NotEqual:
    return "NotEqual";
  case BinaryExpr::Op::Less:
    return "Less";
  case BinaryExpr::Op::LessEqual:
    return "LessEqual";
  case BinaryExpr::Op::Greater:
    return "Greater";
  case BinaryExpr::Op::GreaterEqual:
    return "GreaterEqual";
  case BinaryExpr::Op::LogicalAnd:
    return "LogicalAnd";
  case BinaryExpr::Op::LogicalOr:
    return "LogicalOr";
  }
  return "<invalid-BinaryOp>";
}

// ---------- Post-Sema type rendering ----------
//
// Phase 3 (T031-T033): when an `Expr::inferredType()` is non-null
// the printer renders a ` : <Type>` suffix per
// `emit-ast-format.contract.md` Invariant 2. The Type rendering
// uses an inline classifier rather than depending on
// `nsl::sema::TypeSystem` directly — `nsl-ast` does NOT link
// `nsl-sema` (Principle II §3 layered architecture). The opaque
// `Type*` is queried via lightweight RTTI-style accessors that
// would be best implemented in `nsl-sema` and exposed via a
// callback. For the M3 scaffolding we use `dynamic_cast` against
// the `nsl::sema::Type` hierarchy via the alias declared in
// `Type.h` — this works because `nsl::ast::Type` is a typedef for
// `nsl::sema::Type` post-Phase 3, and the AST printer translation
// unit is allowed to read sema::Type's vtable via the alias.
//
// The inline switch reaches into the shared `TypeKind` enum that
// lives in `nsl/Sema/TypeSystem.h`. To avoid a layer-direction
// violation in CMake, we forward-declare the minimum needed —
// the shared header is read-only and contains no executable code.

} // namespace
} // namespace nsl::ast

// Reach into nsl-sema's TypeSystem header to access the Type
// hierarchy *as a header-only consumer*. No additional CMake
// edge needed: the Sema headers live under `include/nsl/Sema/`
// and the include search path is global.
#include "nsl/Sema/TypeSystem.h"

namespace nsl::ast {
namespace {

/// Render a Sema `TypeRef` as ` : <Type>` per
/// `emit-ast-format.contract.md` Invariant 2. Returns the raw
/// post-suffix string (without the leading space).
void renderTypeSuffix(const ::nsl::sema::Type *t, llvm::raw_ostream &os) {
  if (!t) {
    return;
  }
  os << " : ";
  using K = ::nsl::sema::TypeKind;
  switch (t->kind()) {
  case K::Bit:
    os << "Bit";
    return;
  case K::BitVector: {
    const auto *bv = static_cast<const ::nsl::sema::BitVectorType *>(t);
    os << "BitVector(" << bv->width() << ')';
    return;
  }
  case K::Struct: {
    const auto *st = static_cast<const ::nsl::sema::StructType *>(t);
    os << "Struct(" << st->name() << ')';
    return;
  }
  case K::Memory: {
    const auto *mt = static_cast<const ::nsl::sema::MemoryType *>(t);
    os << "Memory(" << mt->depth() << " x ";
    // Recursively render element type; "x" used in place of "×"
    // to keep the output ASCII for tooling regex-parsability.
    if (mt->element()) {
      // Strip the leading " : " from a recursive call by writing
      // directly here.
      using K2 = ::nsl::sema::TypeKind;
      switch (mt->element()->kind()) {
      case K2::Bit:
        os << "Bit";
        break;
      case K2::BitVector: {
        const auto *bv =
            static_cast<const ::nsl::sema::BitVectorType *>(mt->element());
        os << "BitVector(" << bv->width() << ')';
        break;
      }
      case K2::Struct: {
        const auto *sst =
            static_cast<const ::nsl::sema::StructType *>(mt->element());
        os << "Struct(" << sst->name() << ')';
        break;
      }
      default:
        os << "Unresolved";
        break;
      }
    } else {
      os << "Unresolved";
    }
    os << ')';
    return;
  }
  case K::Unresolved:
    os << "Unresolved";
    return;
  }
  os << "Unresolved";
}

// ---------- The walker ----------

class PrinterVisitor final : public ASTVisitor {
public:
  PrinterVisitor(const SourceManager &sm, llvm::raw_ostream &os,
                 DeclLocLookupFn decl_lookup = nullptr) noexcept
      : sm_(sm), os_(os), decl_lookup_(decl_lookup) {}

  void visit(const CompilationUnit &n) override;
  void visit(const StructDecl &n) override;
  void visit(const TopLevelParamDecl &n) override;
  void visit(const DeclareBlock &n) override;
  void visit(const PortDecl &n) override;
  void visit(const ModuleBlock &n) override;
  void visit(const RegDecl &n) override;
  void visit(const WireDecl &n) override;
  void visit(const VariableDecl &n) override;
  void visit(const IntegerDecl &n) override;
  void visit(const MemDecl &n) override;
  void visit(const FuncSelfDecl &n) override;
  void visit(const ProcNameDecl &n) override;
  void visit(const StateNameDecl &n) override;
  void visit(const FirstStateDecl &n) override;
  void visit(const SubmoduleDecl &n) override;
  void visit(const StructInstDecl &n) override;
  void visit(const FuncDefn &n) override;
  void visit(const ProcDefn &n) override;
  void visit(const StateDefn &n) override;

  void visit(const TransferStmt &n) override;
  void visit(const IncDecStmt &n) override;
  void visit(const ControlCallStmt &n) override;
  void visit(const BareFinishStmt &n) override;
  void visit(const SystemTaskStmt &n) override;
  void visit(const ReturnStmt &n) override;
  void visit(const EmptyStmt &n) override;
  void visit(const LabeledStmt &n) override;
  void visit(const GotoStmt &n) override;
  void visit(const InitBlockStmt &n) override;
  void visit(const DelayTaskStmt &n) override;
  void visit(const ParallelBlock &n) override;
  void visit(const AltBlock &n) override;
  void visit(const AnyBlock &n) override;
  void visit(const SeqBlock &n) override;
  void visit(const WhileBlock &n) override;
  void visit(const ForBlock &n) override;
  void visit(const IfStmt &n) override;
  void visit(const StructuralGenerate &n) override;

  void visit(const LiteralExpr &n) override;
  void visit(const IdentifierExpr &n) override;
  void visit(const SystemVarExpr &n) override;
  void visit(const UnaryExpr &n) override;
  void visit(const BinaryExpr &n) override;
  void visit(const ConditionalExpr &n) override;
  void visit(const ConcatExpr &n) override;
  void visit(const RepeatExpr &n) override;
  void visit(const SignExtendExpr &n) override;
  void visit(const ZeroExtendExpr &n) override;
  void visit(const SliceExpr &n) override;
  void visit(const FieldAccessExpr &n) override;
  void visit(const CallExpr &n) override;
  void visit(const StructCastExpr &n) override;
  void visit(const IncDecExpr &n) override;

private:
  // ---------- Output helpers ----------

  void emitIndent() const {
    for (int i = 0; i < indent_; ++i) {
      os_ << "  ";
    }
  }

  /// Emit `(NodeKind  loc=<range>` ready for kind-specific
  /// fields/children. No trailing space; callers add their own.
  ///
  /// Post-Sema mode (Phase 3 T031-T033): when `n` is an `Expr`
  /// whose `inferredType() != nullptr`, append the additive
  /// ` : <Type>` suffix per `emit-ast-format.contract.md` Invariant
  /// 2 and (for resolvable name-refs) the additive
  /// ` → decl@<file>:<line>:<col>` suffix per Invariant 3. Pre-
  /// Sema mode (Invariant 4) emits the M2 format unchanged.
  void emitOpen(const ASTNode &n) {
    emitIndent();
    os_ << '(' << toString(n.kind()) << "  loc=";
    emitRange(n.loc());
    // Detect post-Sema mode by checking `inferredType()` on Expr
    // nodes. Non-Expr nodes are unaffected.
    if (isExprKind(n.kind())) {
      const Expr &e = static_cast<const Expr &>(n);
      if (e.inferredType() != nullptr) {
        renderTypeSuffix(e.inferredType(), os_);
        // Decl-loc suffix (Invariant 3): only for name-refs whose
        // resolved Symbol* is non-null, AND only when the type is
        // not Unresolved.
        if (decl_lookup_ != nullptr &&
            e.inferredType()->kind() !=
                ::nsl::sema::TypeKind::Unresolved &&
            isNameRefKind(n.kind())) {
          SourceRange decl_range = decl_lookup_(&e);
          if (decl_range.isValid()) {
            auto begin = sm_.resolveVirtual(decl_range.begin());
            os_ << " -> decl@" << begin.path << ':' << begin.line << ':'
                << begin.col;
          }
        }
      }
    }
  }

  /// True iff `k` names a concrete `Expr` subclass.
  static bool isExprKind(NodeKind k) noexcept {
    switch (k) {
    case NodeKind::NK_LiteralExpr:
    case NodeKind::NK_IdentifierExpr:
    case NodeKind::NK_SystemVarExpr:
    case NodeKind::NK_UnaryExpr:
    case NodeKind::NK_BinaryExpr:
    case NodeKind::NK_ConditionalExpr:
    case NodeKind::NK_ConcatExpr:
    case NodeKind::NK_RepeatExpr:
    case NodeKind::NK_SignExtendExpr:
    case NodeKind::NK_ZeroExtendExpr:
    case NodeKind::NK_SliceExpr:
    case NodeKind::NK_FieldAccessExpr:
    case NodeKind::NK_CallExpr:
    case NodeKind::NK_StructCastExpr:
    case NodeKind::NK_IncDecExpr:
      return true;
    default:
      return false;
    }
  }

  /// True iff `k` names a name-ref Expr that may produce a
  /// `→ decl@…` suffix per Invariant 3.
  static bool isNameRefKind(NodeKind k) noexcept {
    return k == NodeKind::NK_IdentifierExpr ||
           k == NodeKind::NK_FieldAccessExpr;
  }

  /// Emit `path:line:col-line:col` in virtual coordinates.
  void emitRange(SourceRange r) const {
    if (!r.isValid()) {
      os_ << "<invalid>";
      return;
    }
    auto begin = sm_.resolveVirtual(r.begin());
    auto end = sm_.resolveVirtual(r.end());
    os_ << begin.path << ':' << begin.line << ':' << begin.col << '-'
        << end.line << ':' << end.col;
  }

  /// Emit `  name=<value>` — leading two-space separator built in.
  /// Identifier values are emitted verbatim (no quoting); the
  /// `Identifier` is a `StringRef` with no embedded whitespace by
  /// construction (lexer guarantee).
  void emitField(llvm::StringRef name, llvm::StringRef value) const {
    os_ << "  " << name << '=' << value;
  }

  void emitField(llvm::StringRef name, unsigned value) const {
    os_ << "  " << name << '=' << value;
  }

  /// Emit a `ScopedName` as `a.b.c`.
  void emitScopedName(llvm::StringRef name, const ScopedName &sn) const {
    os_ << "  " << name << '=';
    for (std::size_t i = 0, e = sn.parts.size(); i < e; ++i) {
      if (i != 0) {
        os_ << '.';
      }
      os_ << sn.parts[i];
    }
  }

  /// Emit a list-of-identifiers as `name=[a,b,c]` with no spaces
  /// between elements.
  void emitNameList(llvm::StringRef field,
                    const std::vector<Identifier> &names) const {
    os_ << "  " << field << '=' << '[';
    for (std::size_t i = 0, e = names.size(); i < e; ++i) {
      if (i != 0) {
        os_ << ',';
      }
      os_ << names[i];
    }
    os_ << ']';
  }

  /// Close `(...)` for a node with no children — same line.
  void closeNoChildren() const { os_ << ')'; }

  /// Emit children, each as a recursive `accept` call. Newlines
  /// separate siblings; the last child's last line absorbs the
  /// parent's closing `)`. Leaves the cursor at end-of-`)` with
  /// NO trailing newline — the caller (or `print()`'s root) appends
  /// the final `\n`.
  ///
  /// Empty `children` is a programming error at this site —
  /// callers should use `closeNoChildren()` for childless nodes.
  void emitChildren(const std::vector<const ASTNode *> &children) {
    os_ << '\n';
    ++indent_;
    for (std::size_t i = 0, e = children.size(); i < e; ++i) {
      if (i != 0) {
        os_ << '\n';
      }
      children[i]->accept(*this);
    }
    --indent_;
    os_ << ')';
  }

  /// Emit a synthetic wrapper line for an aggregate sub-record
  /// (e.g., `StructMember`, `CondCase`). The wrapper has no
  /// `SourceRange` of its own — it inherits its parent's location
  /// for printer roundtrip purposes. The angle brackets in the tag
  /// flag the line as printer-synthetic (Invariant 6 — these are
  /// not `NodeKind` enumerators).
  void emitSyntheticIndent(llvm::StringRef tag) const {
    emitIndent();
    os_ << "(<" << tag << '>';
  }

  const SourceManager &sm_;
  llvm::raw_ostream &os_;
  DeclLocLookupFn decl_lookup_;
  int indent_ = 0;
};

// ---------- visit() implementations ----------

void PrinterVisitor::visit(const CompilationUnit &n) {
  emitOpen(n);
  if (n.items().empty()) {
    closeNoChildren();
    return;
  }
  std::vector<const ASTNode *> kids;
  kids.reserve(n.items().size());
  for (const auto &item : n.items()) {
    kids.push_back(item.get());
  }
  emitChildren(kids);
}

void PrinterVisitor::visit(const StructDecl &n) {
  emitOpen(n);
  emitField("name", n.name());
  if (n.members().empty()) {
    closeNoChildren();
    return;
  }
  // Synthetic wrapper per member: `(<StructMember> name=<id>` with
  // optional width Expr as a child.
  os_ << '\n';
  ++indent_;
  for (std::size_t i = 0, e = n.members().size(); i < e; ++i) {
    const StructMember &m = n.members()[i];
    if (i != 0) {
      os_ << '\n';
    }
    emitSyntheticIndent("StructMember");
    emitField("name", m.name);
    if (m.width) {
      os_ << '\n';
      ++indent_;
      m.width->accept(*this);
      --indent_;
      os_ << ')';
    } else {
      os_ << ')';
    }
  }
  --indent_;
  os_ << ')';
}

void PrinterVisitor::visit(const TopLevelParamDecl &n) {
  emitOpen(n);
  emitField("kind", toString(n.paramKind()));
  emitField("name", n.name());
  if (n.init() == nullptr) {
    closeNoChildren();
    return;
  }
  emitChildren({n.init()});
}

void PrinterVisitor::visit(const DeclareBlock &n) {
  emitOpen(n);
  if (!n.name().empty()) {
    emitField("name", n.name());
  }
  emitField("modifier", toString(n.modifier()));
  std::vector<const ASTNode *> kids;
  kids.reserve(n.headerParams().size() + n.ports().size());
  for (const auto &p : n.headerParams()) {
    kids.push_back(p.get());
  }
  for (const auto &p : n.ports()) {
    kids.push_back(p.get());
  }
  if (kids.empty()) {
    closeNoChildren();
    return;
  }
  emitChildren(kids);
}

void PrinterVisitor::visit(const PortDecl &n) {
  emitOpen(n);
  emitField("direction", toString(n.direction()));
  emitField("name", n.name());
  if (!n.returnTerminal().empty()) {
    emitField("returnTerminal", n.returnTerminal());
  }
  if (!n.dummyArgs().empty()) {
    emitNameList("dummyArgs", n.dummyArgs());
  }
  if (n.width() == nullptr) {
    closeNoChildren();
    return;
  }
  emitChildren({n.width()});
}

void PrinterVisitor::visit(const ModuleBlock &n) {
  emitOpen(n);
  emitField("name", n.name());
  std::vector<const ASTNode *> kids;
  kids.reserve(n.internals().size() + n.actions().size() + n.funcs().size() +
               n.procs().size());
  for (const auto &p : n.internals()) {
    kids.push_back(p.get());
  }
  for (const auto &p : n.actions()) {
    kids.push_back(p.get());
  }
  for (const auto &p : n.funcs()) {
    kids.push_back(p.get());
  }
  for (const auto &p : n.procs()) {
    kids.push_back(p.get());
  }
  if (kids.empty()) {
    closeNoChildren();
    return;
  }
  emitChildren(kids);
}

void PrinterVisitor::visit(const RegDecl &n) {
  emitOpen(n);
  emitField("name", n.name());
  std::vector<const ASTNode *> kids;
  if (n.width() != nullptr) {
    kids.push_back(n.width());
  }
  if (n.init() != nullptr) {
    kids.push_back(n.init());
  }
  if (kids.empty()) {
    closeNoChildren();
    return;
  }
  emitChildren(kids);
}

void PrinterVisitor::visit(const WireDecl &n) {
  emitOpen(n);
  emitField("name", n.name());
  if (n.width() == nullptr) {
    closeNoChildren();
    return;
  }
  emitChildren({n.width()});
}

void PrinterVisitor::visit(const VariableDecl &n) {
  emitOpen(n);
  emitField("name", n.name());
  if (n.width() == nullptr) {
    closeNoChildren();
    return;
  }
  emitChildren({n.width()});
}

void PrinterVisitor::visit(const IntegerDecl &n) {
  emitOpen(n);
  emitField("name", n.name());
  closeNoChildren();
}

void PrinterVisitor::visit(const MemDecl &n) {
  emitOpen(n);
  emitField("name", n.name());
  std::vector<const ASTNode *> kids;
  if (n.depth() != nullptr) {
    kids.push_back(n.depth());
  }
  if (n.width() != nullptr) {
    kids.push_back(n.width());
  }
  for (const auto &v : n.init()) {
    kids.push_back(v.get());
  }
  if (kids.empty()) {
    closeNoChildren();
    return;
  }
  emitChildren(kids);
}

void PrinterVisitor::visit(const FuncSelfDecl &n) {
  emitOpen(n);
  emitField("name", n.name());
  if (!n.returnTerminal().empty()) {
    emitField("returnTerminal", n.returnTerminal());
  }
  if (!n.dummyArgs().empty()) {
    emitNameList("dummyArgs", n.dummyArgs());
  }
  closeNoChildren();
}

void PrinterVisitor::visit(const ProcNameDecl &n) {
  emitOpen(n);
  emitField("name", n.name());
  if (!n.regArgs().empty()) {
    emitNameList("regArgs", n.regArgs());
  }
  closeNoChildren();
}

void PrinterVisitor::visit(const StateNameDecl &n) {
  emitOpen(n);
  emitNameList("names", n.names());
  closeNoChildren();
}

void PrinterVisitor::visit(const FirstStateDecl &n) {
  emitOpen(n);
  emitField("target", n.target());
  closeNoChildren();
}

void PrinterVisitor::visit(const SubmoduleDecl &n) {
  emitOpen(n);
  emitField("templateName", n.templateName());
  if (n.instances().empty() && n.paramAssigns().empty()) {
    closeNoChildren();
    return;
  }
  os_ << '\n';
  ++indent_;
  std::size_t emitted = 0;
  for (const auto &inst : n.instances()) {
    if (emitted != 0) {
      os_ << '\n';
    }
    emitSyntheticIndent("Instance");
    emitField("name", inst.name);
    if (inst.arraySize) {
      os_ << '\n';
      ++indent_;
      inst.arraySize->accept(*this);
      --indent_;
      os_ << ')';
    } else {
      os_ << ')';
    }
    ++emitted;
  }
  for (const auto &pa : n.paramAssigns()) {
    if (emitted != 0) {
      os_ << '\n';
    }
    emitSyntheticIndent("ParamAssign");
    emitField("name", pa.name);
    if (pa.value) {
      os_ << '\n';
      ++indent_;
      pa.value->accept(*this);
      --indent_;
      os_ << ')';
    } else {
      os_ << ')';
    }
    ++emitted;
  }
  --indent_;
  os_ << ')';
}

void PrinterVisitor::visit(const StructInstDecl &n) {
  emitOpen(n);
  emitField("typeName", n.typeName());
  emitField("instanceName", n.instanceName());
  emitField("storage", toString(n.storageKind()));
  std::vector<const ASTNode *> kids;
  if (n.arraySize() != nullptr) {
    kids.push_back(n.arraySize());
  }
  for (const auto &v : n.init()) {
    kids.push_back(v.get());
  }
  if (kids.empty()) {
    closeNoChildren();
    return;
  }
  emitChildren(kids);
}

void PrinterVisitor::visit(const FuncDefn &n) {
  emitOpen(n);
  emitScopedName("name", n.name());
  if (n.body() == nullptr) {
    closeNoChildren();
    return;
  }
  emitChildren({n.body()});
}

void PrinterVisitor::visit(const ProcDefn &n) {
  emitOpen(n);
  emitField("name", n.name());
  if (n.body() == nullptr) {
    closeNoChildren();
    return;
  }
  emitChildren({n.body()});
}

void PrinterVisitor::visit(const StateDefn &n) {
  emitOpen(n);
  emitField("name", n.name());
  if (n.body() == nullptr) {
    closeNoChildren();
    return;
  }
  emitChildren({n.body()});
}

// ---------- Stmts ----------

void PrinterVisitor::visit(const TransferStmt &n) {
  emitOpen(n);
  emitField("op", toString(n.op()));
  std::vector<const ASTNode *> kids;
  if (n.lhs() != nullptr) {
    kids.push_back(n.lhs());
  }
  if (n.rhs() != nullptr) {
    kids.push_back(n.rhs());
  }
  if (kids.empty()) {
    closeNoChildren();
    return;
  }
  emitChildren(kids);
}

void PrinterVisitor::visit(const IncDecStmt &n) {
  emitOpen(n);
  emitField("op", toString(n.op()));
  emitField("prefix", llvm::StringRef(n.prefix() ? "true" : "false"));
  if (n.target() == nullptr) {
    closeNoChildren();
    return;
  }
  emitChildren({n.target()});
}

void PrinterVisitor::visit(const ControlCallStmt &n) {
  emitOpen(n);
  emitScopedName("target", n.target());
  if (n.args().empty()) {
    closeNoChildren();
    return;
  }
  std::vector<const ASTNode *> kids;
  kids.reserve(n.args().size());
  for (const auto &a : n.args()) {
    kids.push_back(a.get());
  }
  emitChildren(kids);
}

void PrinterVisitor::visit(const BareFinishStmt &n) {
  emitOpen(n);
  closeNoChildren();
}

void PrinterVisitor::visit(const SystemTaskStmt &n) {
  emitOpen(n);
  emitField("name", n.name());
  if (n.args().empty()) {
    closeNoChildren();
    return;
  }
  std::vector<const ASTNode *> kids;
  kids.reserve(n.args().size());
  for (const auto &a : n.args()) {
    kids.push_back(a.get());
  }
  emitChildren(kids);
}

void PrinterVisitor::visit(const ReturnStmt &n) {
  emitOpen(n);
  if (n.value() == nullptr) {
    closeNoChildren();
    return;
  }
  emitChildren({n.value()});
}

void PrinterVisitor::visit(const EmptyStmt &n) {
  emitOpen(n);
  closeNoChildren();
}

void PrinterVisitor::visit(const LabeledStmt &n) {
  emitOpen(n);
  emitField("label", n.label());
  if (n.body() == nullptr) {
    closeNoChildren();
    return;
  }
  emitChildren({n.body()});
}

void PrinterVisitor::visit(const GotoStmt &n) {
  emitOpen(n);
  emitField("target", n.target());
  closeNoChildren();
}

void PrinterVisitor::visit(const InitBlockStmt &n) {
  emitOpen(n);
  if (n.items().empty()) {
    closeNoChildren();
    return;
  }
  std::vector<const ASTNode *> kids;
  kids.reserve(n.items().size());
  for (const auto &p : n.items()) {
    kids.push_back(p.get());
  }
  emitChildren(kids);
}

void PrinterVisitor::visit(const DelayTaskStmt &n) {
  emitOpen(n);
  if (n.count() == nullptr) {
    closeNoChildren();
    return;
  }
  emitChildren({n.count()});
}

void PrinterVisitor::visit(const ParallelBlock &n) {
  emitOpen(n);
  if (n.items().empty()) {
    closeNoChildren();
    return;
  }
  std::vector<const ASTNode *> kids;
  kids.reserve(n.items().size());
  for (const auto &p : n.items()) {
    kids.push_back(p.get());
  }
  emitChildren(kids);
}

void PrinterVisitor::visit(const AltBlock &n) {
  emitOpen(n);
  // Render each `CondCase` as a synthetic wrapper containing the
  // guard Expr followed by the body Stmt.
  const bool hasContent = !n.cases().empty() || n.elseCase() != nullptr;
  if (!hasContent) {
    closeNoChildren();
    return;
  }
  os_ << '\n';
  ++indent_;
  std::size_t emitted = 0;
  for (const auto &c : n.cases()) {
    if (emitted != 0) {
      os_ << '\n';
    }
    emitSyntheticIndent("CondCase");
    os_ << '\n';
    ++indent_;
    if (c.cond) {
      c.cond->accept(*this);
      os_ << '\n';
    }
    if (c.body) {
      c.body->accept(*this);
    }
    --indent_;
    os_ << ')';
    ++emitted;
  }
  if (n.elseCase() != nullptr) {
    if (emitted != 0) {
      os_ << '\n';
    }
    emitSyntheticIndent("ElseCase");
    os_ << '\n';
    ++indent_;
    n.elseCase()->accept(*this);
    --indent_;
    os_ << ')';
  }
  --indent_;
  os_ << ')';
}

void PrinterVisitor::visit(const AnyBlock &n) {
  emitOpen(n);
  const bool hasContent = !n.cases().empty() || n.elseCase() != nullptr;
  if (!hasContent) {
    closeNoChildren();
    return;
  }
  os_ << '\n';
  ++indent_;
  std::size_t emitted = 0;
  for (const auto &c : n.cases()) {
    if (emitted != 0) {
      os_ << '\n';
    }
    emitSyntheticIndent("CondCase");
    os_ << '\n';
    ++indent_;
    if (c.cond) {
      c.cond->accept(*this);
      os_ << '\n';
    }
    if (c.body) {
      c.body->accept(*this);
    }
    --indent_;
    os_ << ')';
    ++emitted;
  }
  if (n.elseCase() != nullptr) {
    if (emitted != 0) {
      os_ << '\n';
    }
    emitSyntheticIndent("ElseCase");
    os_ << '\n';
    ++indent_;
    n.elseCase()->accept(*this);
    --indent_;
    os_ << ')';
  }
  --indent_;
  os_ << ')';
}

void PrinterVisitor::visit(const SeqBlock &n) {
  emitOpen(n);
  if (n.items().empty()) {
    closeNoChildren();
    return;
  }
  std::vector<const ASTNode *> kids;
  kids.reserve(n.items().size());
  for (const auto &p : n.items()) {
    kids.push_back(p.get());
  }
  emitChildren(kids);
}

void PrinterVisitor::visit(const WhileBlock &n) {
  emitOpen(n);
  std::vector<const ASTNode *> kids;
  if (n.cond() != nullptr) {
    kids.push_back(n.cond());
  }
  for (const auto &p : n.items()) {
    kids.push_back(p.get());
  }
  if (kids.empty()) {
    closeNoChildren();
    return;
  }
  emitChildren(kids);
}

void PrinterVisitor::visit(const ForBlock &n) {
  emitOpen(n);
  std::vector<const ASTNode *> kids;
  if (n.form().init) {
    kids.push_back(n.form().init.get());
  }
  if (n.form().cond) {
    kids.push_back(n.form().cond.get());
  }
  if (n.form().step) {
    kids.push_back(n.form().step.get());
  }
  for (const auto &p : n.items()) {
    kids.push_back(p.get());
  }
  if (kids.empty()) {
    closeNoChildren();
    return;
  }
  emitChildren(kids);
}

void PrinterVisitor::visit(const IfStmt &n) {
  emitOpen(n);
  std::vector<const ASTNode *> kids;
  if (n.cond() != nullptr) {
    kids.push_back(n.cond());
  }
  if (n.thenBr() != nullptr) {
    kids.push_back(n.thenBr());
  }
  if (n.elseBr() != nullptr) {
    kids.push_back(n.elseBr());
  }
  if (kids.empty()) {
    closeNoChildren();
    return;
  }
  emitChildren(kids);
}

void PrinterVisitor::visit(const StructuralGenerate &n) {
  emitOpen(n);
  emitField("init", n.init());
  std::vector<const ASTNode *> kids;
  if (n.cond() != nullptr) {
    kids.push_back(n.cond());
  }
  if (n.step() != nullptr) {
    kids.push_back(n.step());
  }
  if (n.body() != nullptr) {
    kids.push_back(n.body());
  }
  if (kids.empty()) {
    closeNoChildren();
    return;
  }
  emitChildren(kids);
}

// ---------- Exprs ----------

void PrinterVisitor::visit(const LiteralExpr &n) {
  emitOpen(n);
  emitField("kind", toString(n.litKind()));
  emitField("value", n.spelling());
  if (n.flags() != 0) {
    emitField("flags", static_cast<unsigned>(n.flags()));
  }
  closeNoChildren();
}

void PrinterVisitor::visit(const IdentifierExpr &n) {
  emitOpen(n);
  emitScopedName("name", n.name());
  closeNoChildren();
}

void PrinterVisitor::visit(const SystemVarExpr &n) {
  emitOpen(n);
  emitField("var", toString(n.var()));
  closeNoChildren();
}

void PrinterVisitor::visit(const UnaryExpr &n) {
  emitOpen(n);
  emitField("op", toString(n.op()));
  if (n.sub() == nullptr) {
    closeNoChildren();
    return;
  }
  emitChildren({n.sub()});
}

void PrinterVisitor::visit(const BinaryExpr &n) {
  emitOpen(n);
  emitField("op", toString(n.op()));
  std::vector<const ASTNode *> kids;
  if (n.lhs() != nullptr) {
    kids.push_back(n.lhs());
  }
  if (n.rhs() != nullptr) {
    kids.push_back(n.rhs());
  }
  if (kids.empty()) {
    closeNoChildren();
    return;
  }
  emitChildren(kids);
}

void PrinterVisitor::visit(const ConditionalExpr &n) {
  emitOpen(n);
  std::vector<const ASTNode *> kids;
  if (n.cond() != nullptr) {
    kids.push_back(n.cond());
  }
  if (n.thenE() != nullptr) {
    kids.push_back(n.thenE());
  }
  if (n.elseE() != nullptr) {
    kids.push_back(n.elseE());
  }
  if (kids.empty()) {
    closeNoChildren();
    return;
  }
  emitChildren(kids);
}

void PrinterVisitor::visit(const ConcatExpr &n) {
  emitOpen(n);
  if (n.parts().empty()) {
    closeNoChildren();
    return;
  }
  std::vector<const ASTNode *> kids;
  kids.reserve(n.parts().size());
  for (const auto &p : n.parts()) {
    kids.push_back(p.get());
  }
  emitChildren(kids);
}

void PrinterVisitor::visit(const RepeatExpr &n) {
  emitOpen(n);
  std::vector<const ASTNode *> kids;
  if (n.count() != nullptr) {
    kids.push_back(n.count());
  }
  if (n.body() != nullptr) {
    kids.push_back(n.body());
  }
  if (kids.empty()) {
    closeNoChildren();
    return;
  }
  emitChildren(kids);
}

void PrinterVisitor::visit(const SignExtendExpr &n) {
  emitOpen(n);
  std::vector<const ASTNode *> kids;
  if (n.width() != nullptr) {
    kids.push_back(n.width());
  }
  if (n.sub() != nullptr) {
    kids.push_back(n.sub());
  }
  if (kids.empty()) {
    closeNoChildren();
    return;
  }
  emitChildren(kids);
}

void PrinterVisitor::visit(const ZeroExtendExpr &n) {
  emitOpen(n);
  std::vector<const ASTNode *> kids;
  if (n.width() != nullptr) {
    kids.push_back(n.width());
  }
  if (n.sub() != nullptr) {
    kids.push_back(n.sub());
  }
  if (kids.empty()) {
    closeNoChildren();
    return;
  }
  emitChildren(kids);
}

void PrinterVisitor::visit(const SliceExpr &n) {
  emitOpen(n);
  std::vector<const ASTNode *> kids;
  if (n.sub() != nullptr) {
    kids.push_back(n.sub());
  }
  if (n.hi() != nullptr) {
    kids.push_back(n.hi());
  }
  if (n.lo() != nullptr) {
    kids.push_back(n.lo());
  }
  if (kids.empty()) {
    closeNoChildren();
    return;
  }
  emitChildren(kids);
}

void PrinterVisitor::visit(const FieldAccessExpr &n) {
  emitOpen(n);
  emitField("field", n.field());
  if (n.obj() == nullptr) {
    closeNoChildren();
    return;
  }
  emitChildren({n.obj()});
}

void PrinterVisitor::visit(const CallExpr &n) {
  emitOpen(n);
  emitScopedName("target", n.target());
  if (n.args().empty()) {
    closeNoChildren();
    return;
  }
  std::vector<const ASTNode *> kids;
  kids.reserve(n.args().size());
  for (const auto &a : n.args()) {
    kids.push_back(a.get());
  }
  emitChildren(kids);
}

void PrinterVisitor::visit(const StructCastExpr &n) {
  emitOpen(n);
  emitField("typeName", n.typeName());
  if (!n.memberPath().empty()) {
    emitNameList("memberPath", n.memberPath());
  }
  if (n.sub() == nullptr) {
    closeNoChildren();
    return;
  }
  emitChildren({n.sub()});
}

void PrinterVisitor::visit(const IncDecExpr &n) {
  emitOpen(n);
  emitField("op", toString(n.op()));
  emitField("prefix", llvm::StringRef(n.prefix() ? "true" : "false"));
  if (n.target() == nullptr) {
    closeNoChildren();
    return;
  }
  emitChildren({n.target()});
}

} // namespace

// ---------- Public API ----------

void print(const CompilationUnit &cu, const SourceManager &sm,
           llvm::raw_ostream &os) {
  PrinterVisitor v(sm, os, /*decl_lookup=*/nullptr);
  cu.accept(v);
  os << '\n';
}

void print(const CompilationUnit &cu, const SourceManager &sm,
           llvm::raw_ostream &os, DeclLocLookupFn decl_lookup) {
  PrinterVisitor v(sm, os, decl_lookup);
  cu.accept(v);
  os << '\n';
}

} // namespace nsl::ast
