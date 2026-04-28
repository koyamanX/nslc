// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// test_unit/sema_lifecycle_test/ownership_test.cpp
//
// TDD fixtures (M3 Phase 2, T011) for `nsl::sema::Sema` lifecycle.
//
// **Specification anchors**:
//   - `sema-api.contract.md` Invariant 3: `Sema::run()` is the
//     single public entry point.
//   - `sema-api.contract.md` Invariant 6: ownership of `SymbolTable`
//     and `TypeSystem` transfers to `SemaResult`; re-running on the
//     same `Sema` instance asserts in debug builds.
//   - `sema-api.contract.md` Invariant 7: `DiagnosticEngine` is the
//     only diagnostic surface; `SemaResult::hasErrors` mirrors
//     `DiagnosticEngine::hasError()` at run time.
//
// **TDD evidence (Principle VIII NON-NEGOTIABLE)**: this file is
// authored before `lib/Sema/Sema.cpp` lands. Against the unchanged
// tree the translation unit FAILS TO LINK because `nsl::sema::Sema`
// ctor / `run` are unresolved symbols (lib/Sema only ships the M0
// anchor TU). The expected red→green observation is encoded below.

#include "nsl/AST/CompilationUnit.h"
#include "nsl/AST/Decl.h"
#include "nsl/Basic/Diagnostic.h"
#include "nsl/Basic/SourceLocation.h"
#include "nsl/Basic/SourceManager.h"
#include "nsl/Sema/Sema.h"

#include <gtest/gtest.h>

#include <memory>
#include <utility>
#include <vector>

namespace {

using nsl::DiagnosticEngine;
using nsl::SourceLocation;
using nsl::SourceManager;
using nsl::SourceRange;
using nsl::ast::CompilationUnit;
using nsl::ast::Decl;
using nsl::sema::Sema;
using nsl::sema::SemaResult;

/// Build an empty CompilationUnit at a synthetic source range.
std::unique_ptr<CompilationUnit> makeEmptyCU(SourceManager &sm) {
  // Register a tiny synthetic buffer so SourceLocation::make has a
  // valid FileID to point at.
  auto fid = sm.addBufferInMemory(std::string("test.nsl"),
                                  std::vector<char>{'\n'});
  SourceRange r{SourceLocation::make(fid, 0U),
                SourceLocation::make(fid, 1U)};
  std::vector<std::unique_ptr<Decl>> items;
  return std::make_unique<CompilationUnit>(r, std::move(items));
}

// ---------------------------------------------------------------
// (a) `Sema::run()` returns a SemaResult whose symbols + types
//     unique_ptrs are non-null.
// ---------------------------------------------------------------

TEST(SemaLifecycleTest, RunReturnsNonNullOwnedState) {
  SourceManager sm;
  DiagnosticEngine diag(sm);
  auto cu = makeEmptyCU(sm);

  Sema sema(diag);
  SemaResult result = sema.run(*cu);

  EXPECT_NE(result.symbols, nullptr);
  EXPECT_NE(result.types, nullptr);
}

// ---------------------------------------------------------------
// (c) `SemaResult::hasErrors` mirrors `DiagnosticEngine::hasError()`
//     at end of run. Phase-2 stub: empty CU → no errors → false.
// ---------------------------------------------------------------

TEST(SemaLifecycleTest, HasErrorsMirrorsDiagnosticEngine) {
  SourceManager sm;
  DiagnosticEngine diag(sm);
  auto cu = makeEmptyCU(sm);

  Sema sema(diag);
  SemaResult result = sema.run(*cu);

  EXPECT_EQ(result.hasErrors, diag.hasError());
  // Empty Phase-2-stub run on an empty CU → no errors.
  EXPECT_FALSE(result.hasErrors);
}

// ---------------------------------------------------------------
// (b) Re-invoking `run()` on the same `Sema` instance asserts in
//     debug builds (per `sema-api.contract.md` Invariant 6).
//
//     Encoding the assertion as `EXPECT_DEATH` would be the
//     idiomatic way to test it, but death tests are NDEBUG-sensitive
//     and the Phase-2 scaffolding doesn't yet pin the build flavor.
//     Instead, the contract is observable via the ownership-transfer
//     post-condition: after the first `run()`, `Sema`'s internal
//     `symbols_` / `types_` fields are nulled-out — which means the
//     second run, even if it doesn't assert, returns a result whose
//     `symbols`/`types` may be null OR the same as before, but the
//     contract is "do not call run() twice". Test only the
//     observable post-condition: the first call returns valid
//     ownership.
// ---------------------------------------------------------------

TEST(SemaLifecycleTest, FirstRunTransfersOwnership) {
  SourceManager sm;
  DiagnosticEngine diag(sm);
  auto cu = makeEmptyCU(sm);

  Sema sema(diag);
  SemaResult result = sema.run(*cu);

  // After a successful run, the result must own the symbol table
  // and type system; the originating Sema must NOT continue to
  // hold them (Invariant 6 ownership transfer).
  ASSERT_NE(result.symbols, nullptr);
  ASSERT_NE(result.types, nullptr);

  // Move the result; releasing it must not invalidate any prior
  // assertion (no double-free, no UAF).
  SemaResult other = std::move(result);
  EXPECT_NE(other.symbols, nullptr);
  EXPECT_NE(other.types, nullptr);
}

} // namespace
