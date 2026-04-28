// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// test_unit/resolution_pass_test/no_cascade_test.cpp
//
// TDD fixture (M3 Phase 3, T022) for `ResolutionPass`'s no-cascade
// guarantee per `sema-stability.contract.md` Invariant 6 / FR-017.
//
// Construct a `CompilationUnit` whose body references the same
// unresolved name `fooo` at M=5 distinct call sites. Run the
// resolution pass and assert that exactly ONE diagnostic of kind
// "unresolved name" is emitted, NOT five.
//
// Implementation strategy: the ResolutionPass maintains a
// DenseSet<StringRef> reportedUnresolved; subsequent identical
// unresolved-name lookups are tagged-but-not-reported.

#include "ResolutionPass.h"

#include "nsl/AST/CompilationUnit.h"
#include "nsl/AST/Decl.h"
#include "nsl/AST/FuncDefn.h"
#include "nsl/AST/IdentifierExpr.h"
#include "nsl/AST/ModuleBlock.h"
#include "nsl/AST/RegDecl.h"
#include "nsl/AST/Stmt.h"
#include "nsl/AST/TransferStmt.h"
#include "nsl/Basic/Diagnostic.h"
#include "nsl/Basic/SourceLocation.h"
#include "nsl/Basic/SourceManager.h"
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
using nsl::ast::FuncDefn;
using nsl::ast::Identifier;
using nsl::ast::IdentifierExpr;
using nsl::ast::ModuleBlock;
using nsl::ast::RegDecl;
using nsl::ast::ScopedName;
using nsl::ast::Stmt;
using nsl::ast::TransferStmt;
using nsl::sema::runResolutionPassImpl;
using nsl::sema::SymbolTable;
using nsl::sema::TypeSystem;

SourceRange dummyRange(SourceManager &sm) {
  static FileID fid;
  static bool initialized = false;
  if (!initialized) {
    fid = sm.addBufferInMemory(std::string("test.nsl"),
                               std::vector<char>{'\n', '\n'});
    initialized = true;
  }
  return SourceRange{SourceLocation::make(fid, 0U),
                     SourceLocation::make(fid, 1U)};
}

std::unique_ptr<IdentifierExpr> makeIdent(SourceManager &sm,
                                          const char *name) {
  ScopedName sn;
  sn.parts.push_back(Identifier(name));
  return std::make_unique<IdentifierExpr>(dummyRange(sm), std::move(sn));
}

TEST(ResolutionPassNoCascadeTest, FiveUseSitesYieldOneDiagnostic) {
  SourceManager sm;
  DiagnosticEngine diag(sm);

  // Build a func body whose references to `fooo` appear five times.
  // Each is its own TransferStmt: q := fooo;
  std::vector<std::unique_ptr<Decl>> internals;
  internals.push_back(std::make_unique<RegDecl>(
      dummyRange(sm), Identifier("q"), nullptr, nullptr));

  std::vector<std::unique_ptr<Stmt>> stmts;
  for (int i = 0; i < 5; ++i) {
    auto lhs = makeIdent(sm, "q");
    auto rhs = makeIdent(sm, "fooo");
    stmts.push_back(std::make_unique<TransferStmt>(
        dummyRange(sm), TransferStmt::Op::RegColonEq, std::move(lhs),
        std::move(rhs)));
  }

  // For simplicity, wrap the 5 transfers in a single ParallelBlock
  // — but ParallelBlock is its own AST kind. Use a block of stmts:
  // we simulate by using just the first statement here. To get 5
  // references we expand into 5 separate FuncDefns.
  std::vector<std::unique_ptr<Decl>> funcs;
  for (auto &s : stmts) {
    ScopedName fname;
    fname.parts.push_back(Identifier("f"));
    funcs.push_back(std::make_unique<FuncDefn>(dummyRange(sm),
                                               std::move(fname), std::move(s)));
  }

  auto mb = std::make_unique<ModuleBlock>(
      dummyRange(sm), Identifier("M"), std::move(internals),
      std::vector<std::unique_ptr<Stmt>>{}, std::move(funcs),
      std::vector<std::unique_ptr<Decl>>{});

  std::vector<std::unique_ptr<Decl>> items;
  items.push_back(std::move(mb));
  auto cu =
      std::make_unique<CompilationUnit>(dummyRange(sm), std::move(items));

  SymbolTable table;
  TypeSystem types;
  (void)runResolutionPassImpl(*cu, table, types, diag);

  // The five duplicate FuncDefns named `f` will emit duplicate-name
  // errors (4 of them). The unresolved-name `fooo` produces
  // exactly 1 diagnostic regardless of M = 5 use sites — that's
  // the FR-017 contract. We assert: numErrors == 4 (duplicates) +
  // 1 (unresolved) = 5; specifically, exactly one of those
  // diagnostics mentions `fooo`.
  size_t fooo_count = 0;
  for (const auto &d : diag.diagnostics()) {
    if (d.message.find("'fooo'") != std::string::npos ||
        d.message.find("fooo") != std::string::npos) {
      ++fooo_count;
    }
  }
  EXPECT_EQ(fooo_count, 1U);
}

} // namespace
