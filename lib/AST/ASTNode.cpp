// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// lib/AST/ASTNode.cpp — out-of-line anchor for `ASTNode`'s
// vtable, the `ASTVisitor` vtable, and every concrete node's
// `accept(ASTVisitor&)` override. The override bodies are uniform
// (`visitor.visit(*this);`) and live here so that:
//   1. The concrete-node headers stay short (Principle II §3
//      per-node-kind-header rule with each header ≤60 lines).
//   2. The visitor exhaustiveness invariant (Invariant 5 in
//      `contracts/ast-stability.contract.md`; FR-005) takes effect
//      at LINK time — adding a `NodeKind.def` entry without
//      writing the matching `visit(T&) = 0` override leaves the
//      visitor's vtable referencing an undefined symbol, which
//      surfaces here as a link error against this TU.
//
// Includes are kept alphabetically grouped per family to make
// drift detection (a missing per-kind header) trivial in code
// review.

#include "nsl/AST/ASTNode.h"

#include "nsl/AST/ASTVisitor.h"
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

namespace nsl::ast {

// Vtable anchors for `ASTNode` and `ASTVisitor`. Keeping the dtors
// out-of-line forces every consumer of the polymorphic class
// hierarchy to link against this TU exactly once.
ASTNode::~ASTNode() = default;

ASTVisitor::~ASTVisitor() = default;

// Default `visitDefault` implementation — a no-op. Derived visitors
// that opt into a no-op default for some methods still get the
// no-op behavior; visitors that override this method get their own
// per-class default.
void ASTVisitor::visitDefault(const ASTNode & /*node*/) noexcept {}

// One `accept(ASTVisitor&) const` override per concrete node kind.
// The body is uniform — pick the matching `visit(const T&)`
// overload via the concrete `*this` type. The X-macro expansion
// below is the link-time-exhaustiveness mechanism: a missing
// `visit(const T&) = 0` implementation in a derived visitor leaves
// the line below referencing an undefined symbol.
#define NSL_NODE_KIND(EnumName, BaseClass)                                     \
  void EnumName::accept(ASTVisitor &visitor) const {                           \
    visitor.visit(*this);                                                      \
  }
#include "nsl/AST/NodeKind.def"
#undef NSL_NODE_KIND

} // namespace nsl::ast
