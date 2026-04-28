// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// test_unit/ast_visitor_test/ast_node_construct_test.cpp
//
// TDD fixtures (M2 Phase 2, T005) for the `nsl::ast::ASTNode` base
// class introduced by `specs/005-m2-parser/`.
//
// **Specification anchors**:
//   - FR-018 (`specs/005-m2-parser/spec.md`): every AST node MUST
//     carry a `SourceRange` whose start/end byte-offsets correspond
//     to the full extent of the parsed construct.
//   - Invariant 1 (`specs/005-m2-parser/contracts/ast-stability.contract.md`):
//     "Every concrete `ASTNode` subclass MUST be constructible only
//     with a non-empty `SourceRange`. The base `ASTNode` constructor
//     is protected and takes `(NodeKind, SourceRange)`; no subclass
//     may default-construct a `SourceRange`."
//
// **TDD evidence (Principle VIII NON-NEGOTIABLE)**: this file is
// authored before `include/nsl/AST/ASTNode.h` /
// `include/nsl/AST/NodeKind.h` exist. Against the unchanged tree
// the `#include`s below resolve to no header → translation-unit
// compile failure. The failing-state evidence IS the
// `include/nsl/AST/` directory containing only `.keep` placeholders.
//
// **API surface assumed (from `data-model.md §1.1`)**:
//   - namespace `nsl::ast`
//   - `enum class NodeKind` (generated from `NodeKind.def`)
//   - `class ASTNode { protected: ASTNode(NodeKind, SourceRange);
//                       public:    NodeKind  kind() const;
//                                  SourceRange loc() const; };`
//   - `ASTNode() = delete;`

#include "nsl/AST/ASTNode.h"
#include "nsl/AST/NodeKind.h"
#include "nsl/Basic/SourceLocation.h"

#include "gtest/gtest.h"

#include <type_traits>

using nsl::FileID;
using nsl::SourceLocation;
using nsl::SourceRange;
using nsl::ast::ASTNode;
using nsl::ast::NodeKind;

namespace {

// A test-only concrete subclass exposing the protected `ASTNode`
// constructor. The contract states the base constructor is
// `protected`; concrete `nsl::ast::*` node classes (per data-model
// §§1.2-1.6) inherit it. We need a minimal subclass to exercise the
// constructor in isolation, without depending on the per-node-kind
// header set authored by other Phase-2 tasks (T009/T010/T011).
class TestNode final : public ASTNode {
public:
  TestNode(NodeKind k, SourceRange r) : ASTNode(k, r) {}
  // Track A's `ASTNode::accept()` is pure-virtual; a no-op override
  // makes TestNode concrete. The tests below never invoke accept(),
  // so the body is intentionally empty (TestNode is NOT a real
  // node-kind; it borrows enumerator values for storage testing).
  void accept(::nsl::ast::ASTVisitor &) const override {}
};

// FR-018 Invariant-1 corollary: a default-constructed `ASTNode`
// MUST be `= delete`d so the type system rejects nodes without a
// `SourceRange`. C++17-portable static check (no concept needed).
static_assert(!std::is_default_constructible_v<ASTNode>,
              "ASTNode() must be = delete'd per ast-stability "
              "contract Invariant 1");

// Companion: copy/move construction without a `SourceRange` is also
// forbidden — a copy-constructed node would propagate whatever
// (potentially default-empty) range its source had, which would
// silently break the per-node `SourceRange` invariant. The
// contract itself doesn't pin copy semantics, but the absence of a
// public default constructor is the load-bearing assertion.

TEST(ASTNodeConstructTest, ConstructibleWithSourceRange) {
  // Build a non-empty SourceRange via the M1 SourceManager primitives
  // (data-model entity 1-3 from `specs/002-m1-lex-preprocess/`).
  FileID const fid(1);
  SourceLocation const begin = SourceLocation::make(fid, 0);
  SourceLocation const end = SourceLocation::make(fid, 12);
  SourceRange const range(begin, end);
  ASSERT_TRUE(range.isValid());

  // The constructor pair `(NodeKind, SourceRange)` is the ONLY way to
  // produce an ASTNode (Invariant 1).
  TestNode node(NodeKind::NK_CompilationUnit, range);

  // The constructor MUST store the `SourceRange` faithfully — this
  // is the storage half of FR-018 ("every AST node carries a
  // SourceRange whose start/end byte-offsets correspond to ...").
  EXPECT_EQ(node.loc().begin(), range.begin());
  EXPECT_EQ(node.loc().end(), range.end());
  EXPECT_EQ(node.loc().length(), 12U);
  EXPECT_TRUE(node.loc().isValid());
}

TEST(ASTNodeConstructTest, KindAccessorReflectsConstructorArgument) {
  FileID const fid(1);
  SourceRange const range(SourceLocation::make(fid, 4),
                          SourceLocation::make(fid, 9));
  TestNode node(NodeKind::NK_ModuleBlock, range);

  // `kind()` is the accessor side of the `(NodeKind, SourceRange)`
  // constructor. Pinning this here ensures Invariant 6 (node-kind
  // name stability) has a corresponding *value* stability — the
  // enumerator a node was constructed with is the enumerator it
  // reports.
  EXPECT_EQ(node.kind(), NodeKind::NK_ModuleBlock);
}

TEST(ASTNodeConstructTest, DistinctKindsCarryDistinctRanges) {
  FileID const fid(2);
  SourceRange const r1(SourceLocation::make(fid, 0),
                       SourceLocation::make(fid, 3));
  SourceRange const r2(SourceLocation::make(fid, 5),
                       SourceLocation::make(fid, 11));
  TestNode a(NodeKind::NK_RegDecl, r1);
  TestNode b(NodeKind::NK_LiteralExpr, r2);

  // Construction is independent: each node carries its OWN range.
  EXPECT_EQ(a.loc().begin().offset(), 0U);
  EXPECT_EQ(a.loc().end().offset(), 3U);
  EXPECT_EQ(b.loc().begin().offset(), 5U);
  EXPECT_EQ(b.loc().end().offset(), 11U);
  // And its OWN kind.
  EXPECT_NE(a.kind(), b.kind());
}

} // namespace
