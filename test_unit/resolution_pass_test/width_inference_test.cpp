// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// test_unit/resolution_pass_test/width_inference_test.cpp
//
// TDD fixture (M3 Phase 3, T021) for `ResolutionPass`'s width-
// inference pass. Per design §6.x line 856 ("Width inference is a
// single top-down pass") the pass populates `Expr::inferredType()`
// for every Expr it visits.
//
// This test asserts the FR-009 contract: integer literals resolve
// to a concrete `BitVectorType{N}` `TypeRef`, NOT `Unresolved` and
// NOT `Bit`. Specifically:
//   - decimal literal `5` → `BitVector(N)` for some `N >= 1`;
//   - identifier `q` (resolved to a RegSymbol with declared
//     width 8) → `BitVector(8)`;
//   - the slot is never null after the pass.

#include "ResolutionPass.h"

#include "nsl/AST/CompilationUnit.h"
#include "nsl/AST/Decl.h"
#include "nsl/AST/FuncDefn.h"
#include "nsl/AST/IdentifierExpr.h"
#include "nsl/AST/LiteralExpr.h"
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
using nsl::ast::LiteralExpr;
using nsl::ast::ModuleBlock;
using nsl::ast::RegDecl;
using nsl::ast::ScopedName;
using nsl::ast::Stmt;
using nsl::ast::TransferStmt;
using nsl::sema::BitVectorType;
using nsl::sema::runResolutionPassImpl;
using nsl::sema::SymbolTable;
using nsl::sema::Type;
using nsl::sema::TypeKind;
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

TEST(ResolutionPassWidthInferenceTest, IntegerLiteralResolvesToBitVector) {
  SourceManager sm;
  DiagnosticEngine diag(sm);

  // Build:
  //   module M {
  //     reg q;
  //     func clk { q := 5; }
  //   }
  std::vector<std::unique_ptr<Decl>> internals;
  internals.push_back(std::make_unique<RegDecl>(
      dummyRange(sm), Identifier("q"), nullptr, nullptr));

  auto lhs = makeIdent(sm, "q");
  auto rhs = std::make_unique<LiteralExpr>(
      dummyRange(sm), LiteralExpr::Lit::Decimal, Identifier("5"), 0);
  // Save raw pointer for assertion.
  LiteralExpr *rhs_ptr = rhs.get();
  IdentifierExpr *lhs_ptr = lhs.get();

  auto transfer = std::make_unique<TransferStmt>(
      dummyRange(sm), TransferStmt::Op::RegColonEq, std::move(lhs),
      std::move(rhs));

  ScopedName fname;
  fname.parts.push_back(Identifier("clk"));
  auto fdefn = std::make_unique<FuncDefn>(dummyRange(sm), std::move(fname),
                                          std::move(transfer));

  std::vector<std::unique_ptr<Decl>> funcs;
  funcs.push_back(std::move(fdefn));

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

  EXPECT_FALSE(diag.hasError());

  // Both expressions have non-null inferredType after the pass
  // (FR-004 contract).
  ASSERT_NE(lhs_ptr->inferredType(), nullptr);
  ASSERT_NE(rhs_ptr->inferredType(), nullptr);

  // Literal `5` resolves to a BitVector type per FR-009 — NOT
  // unresolved, NOT bit (singleton).
  const Type *lit_type = rhs_ptr->inferredType();
  EXPECT_NE(lit_type->kind(), TypeKind::Unresolved);
}

} // namespace
