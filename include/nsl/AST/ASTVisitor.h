// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// include/nsl/AST/ASTVisitor.h
//
// `ASTVisitor` — polymorphic double-dispatch visitor for the AST
// (data-model §1.1; FR-005). The base declares one
// **pure-virtual** `visit(T&) = 0` per concrete node kind enumerated
// in `NodeKind.def`. A derived visitor that fails to override any of
// them produces a **link-time** error (its vtable references an
// undefined symbol) — this is the mechanism that satisfies
// `contracts/ast-stability.contract.md` Invariant 5 and FR-005.
//
// Mechanism: Each concrete node's `accept(visitor)` override (defined
// out-of-line in `lib/AST/ASTNode.cpp` via the `NSL_AST_NODE_BOILERPLATE`
// macro from `ASTNode.h`) calls `visitor.visit(*this)` — the type of
// `*this` resolves to the concrete subclass, so the visitor's
// matching `visit(T&)` is selected at compile time.
//
// Optional `visitDefault(ASTNode&)` template hook (research §7):
// derived visitors that want a no-op-by-default behavior write
// per-method overrides forwarding to `visitDefault`. The opt-in is
// explicit (a derived class can't accidentally inherit a silent
// default) — this matches `clang::RecursiveASTVisitor`'s WalkUp*
// pattern.
//
// Forward-declares every concrete node kind from `NodeKind.def` so
// this header has no cycles with the per-node-kind headers. Each
// per-kind header includes `ASTNode.h` (via `Decl.h`/`Stmt.h`/
// `Expr.h`) and indirectly relies on the forward decl here when
// `accept()` is defined.

#ifndef NSL_AST_ASTVISITOR_H
#define NSL_AST_ASTVISITOR_H

#include "nsl/AST/NodeKind.h" // for the enum used by visitDefault dispatch

namespace nsl::ast {

class ASTNode;

// Forward-declare every concrete node so `visit(T&) = 0` can refer
// to it by reference without including the per-kind header.
#define NSL_NODE_KIND(EnumName, BaseClass) class EnumName;
#include "nsl/AST/NodeKind.def"
#undef NSL_NODE_KIND

/// Polymorphic AST visitor.
///
/// Adding a new concrete `NodeKind` forces every derived visitor to
/// either implement the new `visit(T&)` method or explicitly opt
/// into `visitDefault(node)` for it — a missing override is a
/// link-time error.
class ASTVisitor {
public:
  ASTVisitor() = default;
  virtual ~ASTVisitor();

  // Pure-virtual visit overload per concrete node kind. Source order
  // matches `NodeKind.def`. M2's visitor surface is read-only —
  // every `visit(const T&)` takes a `const` reference. Mutating
  // visitors (M3 Sema's name-resolution walker, etc.) will live in
  // a parallel `MutatingASTVisitor` when introduced.
#define NSL_NODE_KIND(EnumName, BaseClass) virtual void visit(const EnumName &node) = 0;
#include "nsl/AST/NodeKind.def"
#undef NSL_NODE_KIND

protected:
  /// Optional default routing hook for derived visitors that want a
  /// no-op default. Derived classes opt in explicitly per method:
  ///
  /// ```cpp
  /// class MyVisitor : public ASTVisitor {
  ///   void visit(const IfStmt &node) override { handleIf(node); }
  ///   void visit(const BinaryExpr &node) override { visitDefault(node); }
  ///   // ... explicit per-method opt-in. No silent defaults.
  /// };
  /// ```
  ///
  /// The base implementation is a no-op; derived visitors MAY
  /// override it for a single common path. M2 ships no overrider —
  /// `Printer` overrides every `visit(T&)` explicitly.
  virtual void visitDefault(const ASTNode &node) noexcept;
};

} // namespace nsl::ast

#endif // NSL_AST_ASTVISITOR_H
