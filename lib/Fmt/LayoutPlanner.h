//===- LayoutPlanner.h - AST → Doc IR visitor ---------------------*- C++ -*-=//
//
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Internal header — NOT exported through `Fmt.h` (Principle II).
//
// Walks a parsed `nsl::ast::CompilationUnit` and produces a Doc IR
// tree (per `lib/Fmt/Doc.h`) that the LayoutRenderer materialises
// into the final formatted byte stream. The visitor is a subclass
// of `nsl::ast::ASTVisitor` so the C++ link-time check enforces
// awareness of every concrete `NodeKind` (Principle I — no silent
// AST drops).
//
// **Phase 3-skeleton scope (this commit)**: every `visit(T&)`
// override emits a single `Doc::text(<verbatim source bytes for
// node.loc()>)`. End-to-end behavior is byte-passthrough, identical
// to the prior raw-rawText emission in `Format.cpp` — no behavior
// change, just architectural plumbing for Phase 3-rules.
//
// **Phase 3-rules path** (T049–T055): one rule at a time, replace
// the verbatim handler for the relevant node kind with a canonical
// Doc-construction:
//   * `visit(BinaryExpr)` → `Doc::concat({lhs, " op ", rhs})` (R5)
//   * `visit(BitSliceExpr)` / `visit(ConcatExpr)` → R4 spacing
//   * `visit(AltBlock)` / `visit(AnyBlock)` → R1 case-arrow alignment
//   * `visit(StructDecl)` → R2 member-bracket alignment
//   * `visit(ProcNameDecl)` → R3 argument-list wrapping
//   * `visit(...)` for trivia handling → R6 attached-comment preservation
// Each replaced override descends manually via helper `visitNode()`
// so subtree recursion uses the new visitors rather than emitting
// the parent's whole verbatim bytes.
//
//===----------------------------------------------------------------------===//

#ifndef NSL_FMT_LIB_LAYOUT_PLANNER_H
#define NSL_FMT_LIB_LAYOUT_PLANNER_H

#include "Doc.h"

#include "nsl/AST/ASTVisitor.h"
#include "nsl/AST/CompilationUnit.h"
#include "nsl/Fmt/Fmt.h"

#include "llvm/ADT/StringRef.h"

namespace nsl::fmt {

/// AST → Doc IR visitor.
///
/// Construction parameters:
///   * `src` — the source bytes the AST was parsed from. Node
///     `loc()` ranges are byte offsets into this buffer; the
///     verbatim handler slices `src` directly. MUST outlive every
///     `build()` call.
///   * `cfg` — the active Configuration (drives the eventual rule
///     decisions; ignored at Phase 3-skeleton).
class LayoutPlanner : public ::nsl::ast::ASTVisitor {
public:
  LayoutPlanner(llvm::StringRef src, const Configuration &cfg) noexcept
      : src_(src), cfg_(cfg) {}

  /// Walk `cu` and produce its Doc representation.
  DocPtr build(const ::nsl::ast::CompilationUnit &cu);

  // Verbatim-fallback visit() override per concrete NodeKind. The
  // macro expands to ~54 method DECLARATIONS (one per
  // `NSL_NODE_KIND` entry in `nsl/AST/NodeKind.def`); the bodies
  // are defined in LayoutPlanner.cpp where each per-node-kind
  // header is included so `node.loc()` resolves to the base
  // `ASTNode::loc()` accessor. Future Phase 3-rules commits
  // replace specific overrides' bodies with canonical-layout logic.
#define NSL_NODE_KIND(EnumName, BaseClass)                                     \
  void visit(const ::nsl::ast::EnumName &node) override;
#include "nsl/AST/NodeKind.def"
#undef NSL_NODE_KIND

private:
  /// Slice `src_` for the given source range and wrap as Doc::text.
  /// Bounds-checked: returns Doc::text("") if the range exceeds
  /// the buffer (shouldn't happen for a well-formed AST but
  /// defends against future refactors).
  DocPtr verbatimFromRange(::nsl::SourceRange r) const;

  llvm::StringRef     src_;
  const Configuration &cfg_;
  DocPtr              result_;
};

} // namespace nsl::fmt

#endif // NSL_FMT_LIB_LAYOUT_PLANNER_H
