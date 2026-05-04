// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// test_unit/constructive_sn_test/s23_test.cc
//
// TDD fixture (M3 Phase 4a, T038) for the **constructive `S23`** —
// a `reg` declared with an init but no explicit width is 1-bit
// wide with the given init value. Per `sema-api.contract.md`
// Invariant 4 + `research.md` §6, the introspection observable is
// `RegSymbol::type() == TypeSystem::bitVector(1)` after Sema runs.
//
// **Phase 4a TDD red state**: at Phase 2 the `Symbol::type()`
// field is initialized to `nullptr`, and Phase 3's ResolutionPass
// sets the type from the declared `width` Expr (which is null for
// the `reg flushing = 0;` shape — so the type stays `nullptr` or
// `unresolved()` until Phase 4b T068 adds the explicit
// `BitVectorType{1}` fallback). The (a) test below asserts the
// post-Sema RegSymbol type is `TypeSystem::bitVector(1)` —
// pre-T068 this FAILS.
//
// The (b) sibling fail-test asserts the OPPOSITE observable
// (`bit()` singleton, NOT `bitVector(1)`) using
// `EXPECT_NONFATAL_FAILURE` per Q1 Option B.

#include "nsl/AST/CompilationUnit.h"
#include "nsl/AST/Decl.h"
#include "nsl/AST/LiteralExpr.h"
#include "nsl/AST/ModuleBlock.h"
#include "nsl/AST/RegDecl.h"
#include "nsl/AST/Stmt.h"
#include "nsl/Basic/Diagnostic.h"
#include "nsl/Basic/SourceLocation.h"
#include "nsl/Basic/SourceManager.h"
#include "nsl/Sema/Sema.h"
#include "nsl/Sema/SymbolTable.h"
#include "nsl/Sema/TypeSystem.h"

#include <gtest/gtest-spi.h>
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
using nsl::ast::LiteralExpr;
using nsl::ast::ModuleBlock;
using nsl::ast::RegDecl;
using nsl::ast::Stmt;
using nsl::sema::RegSymbol;
using nsl::sema::Sema;
using nsl::sema::SemaResult;
using nsl::sema::Symbol;
using nsl::sema::SymbolKind;

SourceRange dummyRange(SourceManager &sm) {
  static FileID fid;
  static bool initialized = false;
  if (!initialized) {
    fid = sm.addBufferInMemory(std::string("s23_test.nsl"),
                               std::vector<char>{'\n', '\n'});
    initialized = true;
  }
  return SourceRange{SourceLocation::make(fid, 0U),
                     SourceLocation::make(fid, 1U)};
}

// Build a CompilationUnit with `module M { reg flushing = 0; }`
// where the reg has no explicit `width`.
std::unique_ptr<CompilationUnit> makeUnitForS23(SourceManager &sm) {
  auto init = std::make_unique<LiteralExpr>(
      dummyRange(sm), LiteralExpr::Lit::Decimal, Identifier("0"), 0U);
  auto reg = std::make_unique<RegDecl>(dummyRange(sm), Identifier("flushing"),
                                       /*width=*/nullptr,
                                       /*init=*/std::move(init));
  std::vector<std::unique_ptr<Decl>> internals;
  internals.push_back(std::move(reg));

  auto mb = std::make_unique<ModuleBlock>(dummyRange(sm), Identifier("M"),
                                          std::move(internals),
                                          std::vector<std::unique_ptr<Stmt>>{},
                                          std::vector<std::unique_ptr<Decl>>{},
                                          std::vector<std::unique_ptr<Decl>>{});
  std::vector<std::unique_ptr<Decl>> items;
  items.push_back(std::move(mb));
  return std::make_unique<CompilationUnit>(dummyRange(sm), std::move(items));
}

// ---------------------------------------------------------------
// (a) S23 pass test — reg with init, no explicit width: post-Sema
//     `RegSymbol::type()` MUST be `TypeSystem::bitVector(1)`.
//     Phase 4b T068 sets this; pre-T068 the assertion FAILS.
// ---------------------------------------------------------------

TEST(ConstructiveS23Test, RegOmittedWidthWithInitIs1Bit) {
  SourceManager sm;
  DiagnosticEngine diag(sm);
  auto cu = makeUnitForS23(sm);

  Sema sema(diag);
  SemaResult r = sema.run(*cu);
  ASSERT_NE(r.symbols, nullptr);
  ASSERT_NE(r.types, nullptr);

  // The reg lives in the module's scope. Phase 3 ResolutionPass
  // walks the module body and declares it; we look up via
  // `lookup` after entering the module scope. Simpler: the Phase
  // 3 ResolutionPass declares all symbols in stack frames and
  // pops; we expose them via `currentScope()` of a freshly-pushed
  // probe scope. The cleanest API for the test is to look up at
  // the global scope: Phase 3 retains `retiredScopes_` so the
  // lookup walks back through them — but only if we re-enter a
  // scope at the right kind.
  //
  // For this Phase-4a test we trust Phase 4b T068 to make the
  // RegSymbol::type() observable directly: any mechanism that
  // exposes the symbol's resolved type is fair game. The
  // assertion below is the *contract*; the precise lookup path
  // may be revised by Phase 4b without changing the contract.
  Symbol *sym = r.symbols->lookup(Identifier("flushing"));
  ASSERT_NE(sym, nullptr) << "Phase 4b T068 must keep RegSymbol "
                             "for `flushing` reachable from the "
                             "post-Sema SymbolTable.";
  ASSERT_EQ(sym->kind(), SymbolKind::SK_Reg);

  auto *reg = static_cast<RegSymbol *>(sym);
  EXPECT_EQ(reg->type(), r.types->bitVector(1))
      << "Phase 4b T068: when RegDecl has init+no-width, the type "
         "MUST be TypeSystem::bitVector(1).";
}

// ---------------------------------------------------------------
// (b) S23 fail-sibling test (Q1 Option B): asserting the type is
//     `bit()` (singleton 1-bit BitType) — instead of
//     `bitVector(1)` — MUST fail. The two are distinct interned
//     types per design §6.x lines 813-818.
// ---------------------------------------------------------------

TEST(ConstructiveS23Test, BitNotBitVectorOneFailsAsExpected) {
  // Build a freestanding TypeSystem to assert the design §6.x
  // distinction independent of Phase 4b's progress.
  nsl::sema::TypeSystem ts;
  EXPECT_NONFATAL_FAILURE(EXPECT_EQ(ts.bit(), ts.bitVector(1)),
                          "Expected equality of these values");
}

} // namespace
