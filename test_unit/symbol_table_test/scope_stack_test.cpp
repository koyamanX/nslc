// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// test_unit/symbol_table_test/scope_stack_test.cpp
//
// TDD fixtures (M3 Phase 2, T005) for `nsl::sema::SymbolTable`.
//
// **Specification anchors**:
//   - data-model §2 (Scope hierarchy): `enterScope`/`leaveScope`
//     stack discipline, six-kind `ScopeKind` enum.
//   - `sema-api.contract.md` Invariant 1: SymbolTable.h is a
//     permitted public header; the `declare`/`lookup`/`lookupScoped`/
//     `enterScope`/`leaveScope`/`currentModule` API is the public
//     surface.
//   - `sema-stability.contract.md` Invariant 2: `Scope::declOrder`
//     iteration matches insertion order (NOT `DenseMap` hash order).
//
// **TDD evidence (Principle VIII NON-NEGOTIABLE)**: this file is
// authored before `lib/Sema/SymbolTable.cpp` lands. Against the
// unchanged tree the translation unit FAILS TO LINK because
// `nsl::sema::SymbolTable` ctor / `enterScope` / `declare` / `lookup`
// / `lookupScoped` are unresolved symbols (lib/Sema only ships the
// M0 anchor TU). The expected red→green observation is encoded in
// the assertions below; the green transition lands when T006/T007
// add the header + impl in the same patch series.

#include "nsl/Basic/SourceLocation.h"
#include "nsl/Sema/SymbolTable.h"

#include <gtest/gtest.h>

#include <memory>
#include <utility>

namespace {

using nsl::SourceLocation;
using nsl::SourceRange;
using nsl::ast::Identifier;
using nsl::sema::PortDirection;
using nsl::sema::PortSymbol;
using nsl::sema::RegSymbol;
using nsl::sema::Scope;
using nsl::sema::ScopeKind;
using nsl::sema::Symbol;
using nsl::sema::SymbolKind;
using nsl::sema::SymbolTable;
using nsl::sema::WireSymbol;

/// Helper: synthesize a non-empty `SourceRange` for a test symbol.
/// The ranges don't have to round-trip to a real buffer for these
/// scaffolding tests — they only have to satisfy `isValid()`.
SourceRange makeRange() {
  return SourceRange{SourceLocation::make(nsl::FileID(1U), 0U),
                     SourceLocation::make(nsl::FileID(1U), 1U)};
}

// ---------------------------------------------------------------
// (a) enterScope / leaveScope balance
// ---------------------------------------------------------------

TEST(SymbolTableScopeStackTest, EnterLeaveBalanced) {
  SymbolTable table;
  EXPECT_EQ(table.scopeDepth(), 0U);

  table.enterScope(ScopeKind::Global);
  EXPECT_EQ(table.scopeDepth(), 1U);

  table.enterScope(ScopeKind::Module);
  EXPECT_EQ(table.scopeDepth(), 2U);

  table.enterScope(ScopeKind::Proc);
  EXPECT_EQ(table.scopeDepth(), 3U);

  table.leaveScope();
  EXPECT_EQ(table.scopeDepth(), 2U);

  table.leaveScope();
  EXPECT_EQ(table.scopeDepth(), 1U);

  table.leaveScope();
  EXPECT_EQ(table.scopeDepth(), 0U);
}

// ---------------------------------------------------------------
// (b) declare() returns false on duplicate name in the current scope
// ---------------------------------------------------------------

TEST(SymbolTableScopeStackTest, DeclareRejectsDuplicateInSameScope) {
  SymbolTable table;
  table.enterScope(ScopeKind::Module);

  EXPECT_TRUE(table.declare(
      std::make_unique<RegSymbol>(Identifier("q"), makeRange())));

  // Same name, same scope → duplicate, must reject.
  EXPECT_FALSE(table.declare(
      std::make_unique<RegSymbol>(Identifier("q"), makeRange())));

  // Different name in same scope → still accepts.
  EXPECT_TRUE(table.declare(
      std::make_unique<WireSymbol>(Identifier("w"), makeRange())));

  table.leaveScope();
}

// ---------------------------------------------------------------
// (c) lookup() walks outward through enclosing scopes
// ---------------------------------------------------------------

TEST(SymbolTableScopeStackTest, LookupWalksOutward) {
  SymbolTable table;
  table.enterScope(ScopeKind::Module);

  ASSERT_TRUE(table.declare(
      std::make_unique<RegSymbol>(Identifier("outer"), makeRange())));

  table.enterScope(ScopeKind::Proc);

  ASSERT_TRUE(table.declare(
      std::make_unique<RegSymbol>(Identifier("inner"), makeRange())));

  // Inner scope sees both inner and outer.
  Symbol *inner = table.lookup(Identifier("inner"));
  ASSERT_NE(inner, nullptr);
  EXPECT_EQ(inner->kind(), SymbolKind::SK_Reg);
  EXPECT_EQ(inner->name(), Identifier("inner"));

  Symbol *outer = table.lookup(Identifier("outer"));
  ASSERT_NE(outer, nullptr);
  EXPECT_EQ(outer->kind(), SymbolKind::SK_Reg);
  EXPECT_EQ(outer->name(), Identifier("outer"));

  // Names not declared anywhere return null.
  EXPECT_EQ(table.lookup(Identifier("missing")), nullptr);

  table.leaveScope();

  // After leaving the Proc scope, only `outer` remains.
  EXPECT_EQ(table.lookup(Identifier("inner")), nullptr);
  EXPECT_NE(table.lookup(Identifier("outer")), nullptr);

  table.leaveScope();
}

// ---------------------------------------------------------------
// (d) lookupScoped(): single-part path resolves identical to
//     lookup() — Phase 2 minimum contract per data-model §2.3.
//     Multi-part SUB.port resolution lands in Phase 3 (T028) once
//     the ResolutionPass populates `SubmoduleSymbol::templateDecl`.
// ---------------------------------------------------------------

TEST(SymbolTableScopeStackTest, LookupScopedSinglePartLikeLookup) {
  SymbolTable table;
  table.enterScope(ScopeKind::Module);

  ASSERT_TRUE(table.declare(
      std::make_unique<PortSymbol>(Identifier("clk"), makeRange(),
                                   PortDirection::Input)));

  nsl::ast::ScopedName name;
  name.parts.push_back(Identifier("clk"));

  Symbol *resolved = table.lookupScoped(name);
  ASSERT_NE(resolved, nullptr);
  EXPECT_EQ(resolved->kind(), SymbolKind::SK_Port);

  // Multi-part name with unresolved head → null at Phase 2 (the
  // Phase-3 ResolutionPass will replace the impl wholesale; the
  // public-API contract is unchanged).
  nsl::ast::ScopedName multi;
  multi.parts.emplace_back("missing_sub");
  multi.parts.emplace_back("port");
  EXPECT_EQ(table.lookupScoped(multi), nullptr);

  table.leaveScope();
}

// ---------------------------------------------------------------
// (e) Iteration order matches Scope::declOrder (insertion order),
//     not DenseMap hash order — `sema-stability.contract.md`
//     Invariant 2.
// ---------------------------------------------------------------

TEST(SymbolTableScopeStackTest, DeclOrderMatchesInsertion) {
  SymbolTable table;
  table.enterScope(ScopeKind::Module);

  ASSERT_TRUE(table.declare(
      std::make_unique<RegSymbol>(Identifier("zebra"), makeRange())));
  ASSERT_TRUE(table.declare(
      std::make_unique<RegSymbol>(Identifier("alpha"), makeRange())));
  ASSERT_TRUE(table.declare(
      std::make_unique<RegSymbol>(Identifier("middle"), makeRange())));

  const Scope *cur = table.currentScope();
  ASSERT_NE(cur, nullptr);

  auto order = cur->declOrder();
  ASSERT_EQ(order.size(), 3U);
  EXPECT_EQ(order[0]->name(), Identifier("zebra"));
  EXPECT_EQ(order[1]->name(), Identifier("alpha"));
  EXPECT_EQ(order[2]->name(), Identifier("middle"));

  table.leaveScope();
}

// ---------------------------------------------------------------
// (f) currentScope() / currentModule() smoke
// ---------------------------------------------------------------

TEST(SymbolTableScopeStackTest, CurrentScopeAndModule) {
  SymbolTable table;
  EXPECT_EQ(table.currentScope(), nullptr);
  EXPECT_EQ(table.currentModule(), nullptr);

  table.enterScope(ScopeKind::Global);
  ASSERT_NE(table.currentScope(), nullptr);
  EXPECT_EQ(table.currentScope()->kind(), ScopeKind::Global);

  table.enterScope(ScopeKind::Module);
  EXPECT_EQ(table.currentScope()->kind(), ScopeKind::Module);

  table.enterScope(ScopeKind::Proc);
  EXPECT_EQ(table.currentScope()->kind(), ScopeKind::Proc);

  // The module owner is null at Phase 2 — `enterScope(Module)` was
  // called with the default null `owner` argument. The
  // ResolutionPass at Phase 3 will pass a non-null
  // `Symbol*` (synthesized for `ModuleBlock`); the contract here is
  // simply that `currentModule()` returns *something* (even if
  // null) without crashing the stack walk.
  (void)table.currentModule();

  table.leaveScope();
  table.leaveScope();
  table.leaveScope();
}

} // namespace
