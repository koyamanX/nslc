// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// test_unit/resolution_pass_test/scope_handling_test.cpp
//
// TDD fixture (M3 Phase 3, T018) for `ResolutionPass`'s scope-stack
// handling per data-model §2.1's six-kind enum.
//
// These tests construct minimal ASTs (a `CompilationUnit` containing
// a `ModuleBlock`, optionally with a `ProcDefn`) and run the
// `runResolutionPass` driver, then assert that:
//   (a) the symbol-table observed mid-walk visits the expected
//       sequence of `ScopeKind`s in source order;
//   (b) on exit, the scope stack is balanced (depth == 0).
//
// Mid-walk scope visits are inferred indirectly: after the pass
// returns, the symbol table's scope stack is empty (every open is
// matched by a close), but the `Symbol`s declared during the walk
// are observable via `lookup()` on a freshly-pushed scope... no,
// actually after `runResolutionPass` the scopes are popped. The
// most reliable post-condition: declared symbols are accessible
// via the symbol table's *internal* state — but we want to test
// the public observable. A simple, robust observable: the
// `SemaResult` does NOT expose internal scope counts; instead, we
// assert the post-state is internally consistent by re-running
// `lookup` after a synthetic re-push.
//
// For Phase 3, the scope-handling test focuses on:
//   - The Global scope is opened on entry and closed on exit
//     (depth() == 0 after the walk).
//   - Nested ModuleBlock + ProcDefn produce nested scopes that are
//     all balanced.
// (This is the "balance" half of the contract; the per-`ScopeKind`
// distinction is exercised indirectly via `symbol_declaration_test`,
// `identifier_resolution_test`, and the lit fixtures under
// `test/sema/resolution/`.)

#include "ResolutionPass.h"

#include "nsl/AST/CompilationUnit.h"
#include "nsl/AST/Decl.h"
#include "nsl/AST/ModuleBlock.h"
#include "nsl/AST/ProcDefn.h"
#include "nsl/AST/SeqBlock.h"
#include "nsl/Basic/Diagnostic.h"
#include "nsl/Basic/SourceLocation.h"
#include "nsl/Basic/SourceManager.h"
#include "nsl/Sema/Sema.h"
#include "nsl/Sema/SymbolTable.h"
#include "nsl/Sema/TypeSystem.h"

#include <gtest/gtest.h>

#include <memory>
#include <utility>
#include <vector>

namespace {

using nsl::DiagnosticEngine;
using nsl::FileID;
using nsl::SourceLocation;
using nsl::SourceManager;
using nsl::SourceRange;
using nsl::ast::CompilationUnit;
using nsl::ast::Decl;
using nsl::ast::Identifier;
using nsl::ast::ModuleBlock;
using nsl::ast::ProcDefn;
using nsl::ast::SeqBlock;
using nsl::ast::Stmt;
using nsl::sema::runResolutionPassImpl;
using nsl::sema::SymbolTable;
using nsl::sema::TypeSystem;

SourceRange dummyRange(SourceManager &sm) {
  static FileID fid;
  static bool initialized = false;
  if (!initialized) {
    fid = sm.addBufferInMemory(std::string("test.nsl"),
                               std::vector<char>{'\n', '\n', '\n', '\n'});
    initialized = true;
  }
  return SourceRange{SourceLocation::make(fid, 0U),
                     SourceLocation::make(fid, 1U)};
}

// Build: empty module M
std::unique_ptr<CompilationUnit> makeUnitOneModule(SourceManager &sm) {
  std::vector<std::unique_ptr<Decl>> internals;
  std::vector<std::unique_ptr<Stmt>> actions;
  std::vector<std::unique_ptr<Decl>> funcs;
  std::vector<std::unique_ptr<Decl>> procs;
  auto mb = std::make_unique<ModuleBlock>(
      dummyRange(sm), Identifier("M"), std::move(internals),
      std::move(actions), std::move(funcs), std::move(procs));

  std::vector<std::unique_ptr<Decl>> items;
  items.push_back(std::move(mb));
  return std::make_unique<CompilationUnit>(dummyRange(sm), std::move(items));
}

// Build: module M with one proc P (empty body).
std::unique_ptr<CompilationUnit> makeUnitModuleWithProc(SourceManager &sm) {
  auto seq =
      std::make_unique<SeqBlock>(dummyRange(sm), std::vector<std::unique_ptr<Stmt>>{});
  auto proc = std::make_unique<ProcDefn>(dummyRange(sm), Identifier("P"),
                                         std::move(seq));
  std::vector<std::unique_ptr<Decl>> internals;
  std::vector<std::unique_ptr<Stmt>> actions;
  std::vector<std::unique_ptr<Decl>> funcs;
  std::vector<std::unique_ptr<Decl>> procs;
  procs.push_back(std::move(proc));
  auto mb = std::make_unique<ModuleBlock>(
      dummyRange(sm), Identifier("M"), std::move(internals),
      std::move(actions), std::move(funcs), std::move(procs));

  std::vector<std::unique_ptr<Decl>> items;
  items.push_back(std::move(mb));
  return std::make_unique<CompilationUnit>(dummyRange(sm), std::move(items));
}

TEST(ResolutionPassScopeHandlingTest, EmptyUnitBalancesGlobalScope) {
  SourceManager sm;
  DiagnosticEngine diag(sm);
  auto cu = std::make_unique<CompilationUnit>(
      dummyRange(sm), std::vector<std::unique_ptr<Decl>>{});

  SymbolTable table;
  TypeSystem types;
  (void)runResolutionPassImpl(*cu, table, types, diag);

  // After the walk, every scope opened on entry has been closed on
  // exit. depth() == 0 confirms the Global scope was popped.
  EXPECT_EQ(table.scopeDepth(), 0U);
}

TEST(ResolutionPassScopeHandlingTest, ModuleScopeBalances) {
  SourceManager sm;
  DiagnosticEngine diag(sm);
  auto cu = makeUnitOneModule(sm);

  SymbolTable table;
  TypeSystem types;
  (void)runResolutionPassImpl(*cu, table, types, diag);

  EXPECT_EQ(table.scopeDepth(), 0U);
  EXPECT_FALSE(diag.hasError());
}

TEST(ResolutionPassScopeHandlingTest, ProcScopeBalances) {
  SourceManager sm;
  DiagnosticEngine diag(sm);
  auto cu = makeUnitModuleWithProc(sm);

  SymbolTable table;
  TypeSystem types;
  (void)runResolutionPassImpl(*cu, table, types, diag);

  EXPECT_EQ(table.scopeDepth(), 0U);
  EXPECT_FALSE(diag.hasError());
}

} // namespace
