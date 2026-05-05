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

#include <cstdint>

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
  std::uint32_t begin = r.begin().offset();
  std::uint32_t end = r.end().offset();
  if (begin >= src_.size() || end > src_.size() || begin > end) {
    return Doc::text(llvm::StringRef{});
  }
  return Doc::text(src_.substr(begin, end - begin));
}

// Verbatim-fallback method definitions, one per concrete NodeKind.
// Macro-generated to track NodeKind.def: a new node kind added to
// the spec adds one declaration in LayoutPlanner.h (via the same
// macro pattern in the header) and one definition here. Phase 3-rules
// commits replace specific bodies with canonical Doc construction.
#define NSL_NODE_KIND(EnumName, BaseClass)                                     \
  void LayoutPlanner::visit(const ::nsl::ast::EnumName &node) {                \
    result_ = verbatimFromRange(node.loc());                                   \
  }
#include "nsl/AST/NodeKind.def"
#undef NSL_NODE_KIND

} // namespace nsl::fmt
