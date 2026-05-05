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
#include "nsl/AST/BinaryExpr.h" // for nested `BinaryExpr::Op`
#include "nsl/AST/CompilationUnit.h"
#include "nsl/AST/UnaryExpr.h"  // for nested `UnaryExpr::Op`
#include "nsl/Fmt/Fmt.h"
// `SliceExpr`, `ConcatExpr`, `WireDecl`, etc. are forward-declared by
// `ASTVisitor.h`'s NodeKind.def expansion above — no extra includes
// needed in this header. The full definitions are pulled in by
// `LayoutPlanner.cpp` (which already includes every per-NodeKind
// header so the macro-generated `visit()` bodies can call
// `node.loc()`).

#include "llvm/ADT/ArrayRef.h"
#include "llvm/ADT/StringRef.h"

#include <cstdint>

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

  // visit() override per concrete NodeKind. The macro expands to ~54
  // method DECLARATIONS (one per `NSL_NODE_KIND` entry in
  // `nsl/AST/NodeKind.def`); each body just calls `result_ =
  // formatNode(node)` and overload resolution selects the most-
  // specific `formatNode(const T&)` overload below — defaulting to
  // the verbatim fallback when no per-NodeKind overload is declared.
  // This pattern lets Phase 3-rules commits add canonical layouts
  // one NodeKind at a time without the link-time visitor surface
  // ever drifting (every `NSL_NODE_KIND` entry still gets a unique
  // `visit()` definition; we just centralise the dispatch).
#define NSL_NODE_KIND(EnumName, BaseClass)                                     \
  void visit(const ::nsl::ast::EnumName &node) override;
#include "nsl/AST/NodeKind.def"
#undef NSL_NODE_KIND

protected:
  /// Default verbatim-fallback. Selected by overload resolution when
  /// no more-specific `formatNode(const T&)` overload exists for the
  /// concrete NodeKind being formatted (i.e., the implicit
  /// derived-to-base conversion to `const ASTNode&` is required).
  DocPtr formatNode(const ::nsl::ast::ASTNode &node);

  // Per-NodeKind canonical-layout overrides. T049 R5 (binary/unary
  // operator spacing) lands the first five — the `CompilationUnit`,
  // `ModuleBlock`, and `RegDecl` overrides exist solely to recurse
  // into descendants so the `BinaryExpr` / `UnaryExpr` overrides
  // actually fire on the relevant nested nodes (the verbatim
  // fallback would otherwise emit the parent's whole byte range,
  // bypassing the rule). Subsequent rule tasks (T050–T055) extend
  // this list one rule at a time per Principle VIII TDD discipline.
  DocPtr formatNode(const ::nsl::ast::CompilationUnit &node);
  DocPtr formatNode(const ::nsl::ast::ModuleBlock &node);
  DocPtr formatNode(const ::nsl::ast::RegDecl &node);
  DocPtr formatNode(const ::nsl::ast::WireDecl &node);
  DocPtr formatNode(const ::nsl::ast::BinaryExpr &node);
  DocPtr formatNode(const ::nsl::ast::UnaryExpr &node);
  DocPtr formatNode(const ::nsl::ast::SliceExpr &node);
  DocPtr formatNode(const ::nsl::ast::ConcatExpr &node);

  /// Spelling table for `BinaryExpr::Op`. Returns a static-storage
  /// `StringRef` so the result outlives any Doc tree built from it.
  static llvm::StringRef binaryOpSpelling(::nsl::ast::BinaryExpr::Op op);

  /// Spelling table for `UnaryExpr::Op`. Same lifetime guarantee.
  static llvm::StringRef unaryOpSpelling(::nsl::ast::UnaryExpr::Op op);

  /// Visit a single child node and return its Doc representation.
  /// Future Phase 3-rules overrides (T049-T055) use this to recurse
  /// into specific sub-nodes (e.g., `RegDecl::init()`) without
  /// emitting the parent's whole range verbatim. Save/restore the
  /// `result_` member so nested calls compose naturally:
  ///
  ///   ```cpp
  ///   void visit(const RegDecl& node) override {
  ///     result_ = Doc::concat({
  ///       verbatimFromRange(prefix_range),     // "reg <name>[<width>] = "
  ///       visitNode(*node.init()),             // recurse — R5 fires here
  ///       verbatimFromRange(suffix_range),     // ";"
  ///     });
  ///   }
  ///   ```
  DocPtr visitNode(const ::nsl::ast::ASTNode &child);

  /// Slice `src_` for the given source range and wrap as Doc::text.
  /// Bounds-checked: returns Doc::text("") if the range exceeds
  /// the buffer (shouldn't happen for a well-formed AST but
  /// defends against future refactors).
  DocPtr verbatimFromRange(::nsl::SourceRange r) const;

  /// Slice `src_` for [begin, end) byte offsets and wrap as
  /// Doc::text. Used by `interleaveChildren` for the inter-child
  /// gaps.
  DocPtr verbatimFromOffsets(std::uint32_t begin,
                             std::uint32_t end) const;

  /// Emit a parent node by interleaving verbatim source bytes
  /// (between children) with recursive child visits via
  /// `visitNode()`. The `children` list MUST be in source-position
  /// order (sorted ascending by `loc().begin().offset()`); duplicate
  /// or out-of-order children produce ill-formed output.
  ///
  /// Used by parent visitors (CompilationUnit, ModuleBlock,
  /// RegDecl, etc.) so canonical-layout child overrides
  /// (BinaryExpr, UnaryExpr, ...) can fire on nested nodes.
  DocPtr interleaveChildren(
      ::nsl::SourceRange parent_range,
      llvm::ArrayRef<const ::nsl::ast::ASTNode *> children);

  /// Direct access to the source buffer + active config for
  /// derived rule visitors that need finer-grained slicing than
  /// `verbatimFromRange()` provides.
  [[nodiscard]] llvm::StringRef sourceBuffer() const noexcept { return src_; }
  [[nodiscard]] const Configuration &config() const noexcept { return cfg_; }

private:
  llvm::StringRef src_;
  const Configuration &cfg_;
  DocPtr result_;
};

} // namespace nsl::fmt

#endif // NSL_FMT_LIB_LAYOUT_PLANNER_H
