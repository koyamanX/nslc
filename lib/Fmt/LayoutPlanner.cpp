// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// lib/Fmt/LayoutPlanner.cpp — Phase 3-skeleton implementation.
// Verbatim-fallback only; Phase 3-rules replaces per-node-kind
// handlers incrementally.

#include "LayoutPlanner.h"

#include "Doc.h"

// Per-node-kind AST headers — every concrete NodeKind needs a
// full definition so the macro-generated `visit(const Foo& node)`
// body can call `node.loc()` (which dispatches to ASTNode::loc()
// via the static type of `node`). Same pattern as
// `lib/Lower/ASTToMLIR.cpp`.
#include "nsl/AST/AltBlock.h"
#include "nsl/AST/AnyBlock.h"
#include "nsl/AST/BareFinishStmt.h"
#include "nsl/AST/BinaryExpr.h"
#include "nsl/AST/CallExpr.h"
#include "nsl/AST/CompilationUnit.h"
#include "nsl/AST/ConcatExpr.h"
#include "nsl/AST/ConditionalExpr.h"
#include "nsl/AST/ControlCallStmt.h"
#include "nsl/AST/Decl.h"
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

#include "nsl/Basic/SourceLocation.h"

#include "llvm/ADT/StringRef.h"

#include <algorithm>
#include <cstdint>
#include <vector>

namespace nsl::fmt {

DocPtr LayoutPlanner::build(const ::nsl::ast::CompilationUnit &cu) {
  result_ = nullptr;
  cu.accept(*this); // dispatches to visit(CompilationUnit)
  return result_ ? result_ : Doc::text(llvm::StringRef{});
}

DocPtr LayoutPlanner::visitNode(const ::nsl::ast::ASTNode &child) {
  // Save/restore `result_` so nested calls compose naturally —
  // each `accept()` writes its result into `result_`, and a parent
  // visitor that calls `visitNode(*child)` to recurse into a
  // specific sub-node mustn't lose its own in-progress Doc.
  DocPtr saved = std::move(result_);
  result_ = nullptr;
  child.accept(*this);
  DocPtr child_doc = std::move(result_);
  result_ = std::move(saved);
  return child_doc ? child_doc : Doc::text(llvm::StringRef{});
}

DocPtr LayoutPlanner::verbatimFromRange(::nsl::SourceRange r) const {
  if (!r.begin().isValid() || !r.end().isValid()) {
    return Doc::text(llvm::StringRef{});
  }
  return verbatimFromOffsets(r.begin().offset(), r.end().offset());
}

DocPtr LayoutPlanner::verbatimFromOffsets(std::uint32_t begin,
                                          std::uint32_t end) const {
  if (begin > end || end > src_.size()) {
    return Doc::text(llvm::StringRef{});
  }
  if (begin == end) {
    return Doc::text(llvm::StringRef{});
  }
  return Doc::text(src_.substr(begin, end - begin));
}

DocPtr LayoutPlanner::interleaveChildren(
    ::nsl::SourceRange parent_range,
    llvm::ArrayRef<const ::nsl::ast::ASTNode *> children) {
  std::vector<DocPtr> parts;
  std::uint32_t cursor = parent_range.begin().offset();
  const std::uint32_t parent_end = parent_range.end().offset();
  for (const ::nsl::ast::ASTNode *child : children) {
    if (child == nullptr) {
      continue;
    }
    ::nsl::SourceRange cr = child->loc();
    std::uint32_t child_begin = cr.begin().offset();
    std::uint32_t child_end = cr.end().offset();
    if (child_begin < cursor) {
      // Out-of-order or overlapping child — bail safely with
      // verbatim emission of the whole parent range. (Should not
      // happen for a well-formed AST.)
      return verbatimFromRange(parent_range);
    }
    if (child_begin > cursor) {
      parts.push_back(verbatimFromOffsets(cursor, child_begin));
    }
    parts.push_back(visitNode(*child));
    cursor = child_end;
  }
  if (cursor < parent_end) {
    parts.push_back(verbatimFromOffsets(cursor, parent_end));
  }
  return Doc::concat(std::move(parts));
}

// `visit()` body per concrete NodeKind. Each just routes to
// `formatNode(node)`, where overload resolution selects the most-
// specific overload available — defaulting to the verbatim fallback
// `formatNode(const ASTNode&)` when no per-NodeKind override exists.
// This pattern keeps the link-time visitor surface honest (every
// `NSL_NODE_KIND` entry still gets a dedicated `visit()` definition,
// so a missing override is a link-time error per Principle I) while
// letting Phase 3-rules tasks add canonical layouts via plain
// function overloads — no per-NodeKind macro skip-list needed.
#define NSL_NODE_KIND(EnumName, BaseClass)                                     \
  void LayoutPlanner::visit(const ::nsl::ast::EnumName &node) {                \
    result_ = formatNode(node);                                                \
  }
#include "nsl/AST/NodeKind.def"
#undef NSL_NODE_KIND

// ---- Default verbatim-fallback ------------------------------------------

DocPtr LayoutPlanner::formatNode(const ::nsl::ast::ASTNode &node) {
  return verbatimFromRange(node.loc());
}

// ---- Per-NodeKind canonical-layout overrides ----------------------------
//
// T049 R5 (operator spacing). The `CompilationUnit`/`ModuleBlock`/
// `RegDecl` overrides exist to recurse into descendants so the
// `BinaryExpr` / `UnaryExpr` overrides actually fire on nested
// expression nodes — without them, the verbatim parent fallback would
// emit the whole `module ... { reg x[8] = a+b; }` byte range and the
// inner R5 rule would never see the `a+b`.

DocPtr
LayoutPlanner::formatNode(const ::nsl::ast::CompilationUnit &node) {
  std::vector<const ::nsl::ast::ASTNode *> children;
  children.reserve(node.items().size());
  for (const auto &item : node.items()) {
    children.push_back(item.get());
  }
  return interleaveChildren(node.loc(), children);
}

DocPtr LayoutPlanner::formatNode(const ::nsl::ast::ModuleBlock &node) {
  // Four declaration-order vectors (per ModuleBlock.h) merge into a
  // single source-position-sorted list so `interleaveChildren` can
  // walk them as a flat sequence. The four vectors are sorted by
  // KIND, not source order, so we sort here.
  std::vector<const ::nsl::ast::ASTNode *> children;
  children.reserve(node.internals().size() + node.actions().size() +
                   node.funcs().size() + node.procs().size());
  for (const auto &n : node.internals()) {
    children.push_back(n.get());
  }
  for (const auto &n : node.actions()) {
    children.push_back(n.get());
  }
  for (const auto &n : node.funcs()) {
    children.push_back(n.get());
  }
  for (const auto &n : node.procs()) {
    children.push_back(n.get());
  }
  std::sort(children.begin(), children.end(),
            [](const ::nsl::ast::ASTNode *a, const ::nsl::ast::ASTNode *b) {
              return a->loc().begin().offset() < b->loc().begin().offset();
            });
  return interleaveChildren(node.loc(), children);
}

DocPtr LayoutPlanner::formatNode(const ::nsl::ast::RegDecl &node) {
  // `width` precedes `init` in the source (`reg <name>[<width>] =
  // <init>;`); rely on the AST's lexical order rather than re-sorting.
  std::vector<const ::nsl::ast::ASTNode *> children;
  if (node.width() != nullptr) {
    children.push_back(node.width());
  }
  if (node.init() != nullptr) {
    children.push_back(node.init());
  }
  return interleaveChildren(node.loc(), children);
}

DocPtr LayoutPlanner::formatNode(const ::nsl::ast::BinaryExpr &node) {
  // R5: one space on each side of the binary operator. Recurse into
  // lhs/rhs via `visitNode` so nested binary/unary subtrees get the
  // same canonicalisation.
  DocPtr lhs_doc = node.lhs() != nullptr ? visitNode(*node.lhs())
                                          : Doc::text(llvm::StringRef{});
  DocPtr rhs_doc = node.rhs() != nullptr ? visitNode(*node.rhs())
                                          : Doc::text(llvm::StringRef{});
  std::vector<DocPtr> parts;
  parts.reserve(5);
  parts.push_back(std::move(lhs_doc));
  parts.push_back(Doc::text(llvm::StringRef(" ")));
  parts.push_back(Doc::text(binaryOpSpelling(node.op())));
  parts.push_back(Doc::text(llvm::StringRef(" ")));
  parts.push_back(std::move(rhs_doc));
  return Doc::concat(std::move(parts));
}

DocPtr LayoutPlanner::formatNode(const ::nsl::ast::UnaryExpr &node) {
  // R5: no space between the unary operator and its operand. The
  // operator immediately abuts the recursively-formatted operand.
  DocPtr sub_doc = node.sub() != nullptr ? visitNode(*node.sub())
                                          : Doc::text(llvm::StringRef{});
  std::vector<DocPtr> parts;
  parts.reserve(2);
  parts.push_back(Doc::text(unaryOpSpelling(node.op())));
  parts.push_back(std::move(sub_doc));
  return Doc::concat(std::move(parts));
}

// ---- Op-spelling tables -------------------------------------------------

llvm::StringRef
LayoutPlanner::binaryOpSpelling(::nsl::ast::BinaryExpr::Op op) {
  using Op = ::nsl::ast::BinaryExpr::Op;
  switch (op) {
  case Op::Add:
    return "+";
  case Op::Sub:
    return "-";
  case Op::Mul:
    return "*";
  case Op::Div:
    return "/";
  case Op::Mod:
    return "%";
  case Op::BitAnd:
    return "&";
  case Op::BitOr:
    return "|";
  case Op::BitXor:
    return "^";
  case Op::ShiftLeft:
    return "<<";
  case Op::ShiftRight:
    return ">>";
  case Op::Equal:
    return "==";
  case Op::NotEqual:
    return "!=";
  case Op::Less:
    return "<";
  case Op::LessEqual:
    return "<=";
  case Op::Greater:
    return ">";
  case Op::GreaterEqual:
    return ">=";
  case Op::LogicalAnd:
    return "&&";
  case Op::LogicalOr:
    return "||";
  }
  return llvm::StringRef{};
}

llvm::StringRef
LayoutPlanner::unaryOpSpelling(::nsl::ast::UnaryExpr::Op op) {
  using Op = ::nsl::ast::UnaryExpr::Op;
  switch (op) {
  case Op::Neg:
    return "-";
  case Op::Plus:
    return "+";
  case Op::BitNot:
    return "~";
  case Op::LogicalNot:
    return "!";
  case Op::ReduceAnd:
    return "&";
  case Op::ReduceOr:
    return "|";
  case Op::ReduceXor:
    return "^";
  }
  return llvm::StringRef{};
}

} // namespace nsl::fmt
