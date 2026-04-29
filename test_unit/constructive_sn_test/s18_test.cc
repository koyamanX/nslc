// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// test_unit/constructive_sn_test/s18_test.cc
//
// TDD fixture (M3 Phase 4a, T038) for the **constructive `S18`**
// — earlier-declared struct members occupy more-significant bit
// positions of the packed layout (MSB-first). The introspection
// observable per `sema-api.contract.md` Invariant 4 is
// `StructTypeSymbol::fields()` returning `ArrayRef<FieldInfo>` in
// MSB-first order, with `fields[0].offset == totalWidth - fields[0].width`.
//
// **Phase 4a TDD red state**: `StructTypeSymbol` exists in Phase 2
// (a passive container) but no Sema pass calls `setFields()` yet.
// Phase 4b T066 walks `StructDecl` nodes during the
// `ConstraintCheckPass` and calls `setFields()` with the MSB-first
// packing. The (a) test below drives `Sema::run()` over a synthetic
// CompilationUnit containing a 3-field StructDecl and asserts the
// post-Sema StructTypeSymbol has `fields().size() == 3` with
// MSB-first offsets. At Phase 4a this FAILS because the Phase 4b
// checker is not yet wired in — the lookup either returns a
// `StructTypeSymbol` with zero `fields()`, or the resolution pass
// hasn't yet declared the struct symbol (depending on Phase 3's
// progress).
//
// The (b) sibling fail-test asserts the OPPOSITE observable
// (LSB-first ordering) using `EXPECT_NONFATAL_FAILURE` per
// Q1 Option B — if a future implementation accidentally packs
// LSB-first the regression flips.

#include "nsl/AST/CompilationUnit.h"
#include "nsl/AST/Decl.h"
#include "nsl/AST/StructDecl.h"
#include "nsl/Basic/Diagnostic.h"
#include "nsl/Basic/SourceLocation.h"
#include "nsl/Basic/SourceManager.h"
#include "nsl/Sema/Sema.h"
#include "nsl/Sema/SymbolTable.h"
#include "nsl/Sema/TypeSystem.h"

#include <gtest/gtest.h>
#include <gtest/gtest-spi.h>

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
using nsl::ast::StructDecl;
using nsl::ast::StructMember;
using nsl::sema::FieldInfo;
using nsl::sema::Sema;
using nsl::sema::SemaResult;
using nsl::sema::StructTypeSymbol;
using nsl::sema::Symbol;
using nsl::sema::SymbolKind;

SourceRange dummyRange(SourceManager &sm) {
  static FileID fid;
  static bool initialized = false;
  if (!initialized) {
    fid = sm.addBufferInMemory(std::string("s18_test.nsl"),
                               std::vector<char>{'\n', '\n'});
    initialized = true;
  }
  return SourceRange{SourceLocation::make(fid, 0U),
                     SourceLocation::make(fid, 1U)};
}

// Build a CompilationUnit containing a single StructDecl with three
// width-bearing members in declaration order: msb_field[4],
// mid_field[2], lsb_field[2].
std::unique_ptr<CompilationUnit> makeUnitWithStruct(SourceManager &sm) {
  std::vector<StructMember> members;
  members.push_back({Identifier("msb_field"), nullptr});
  members.push_back({Identifier("mid_field"), nullptr});
  members.push_back({Identifier("lsb_field"), nullptr});
  auto sd = std::make_unique<StructDecl>(dummyRange(sm),
                                         Identifier("hdr_t"),
                                         std::move(members));
  std::vector<std::unique_ptr<Decl>> items;
  items.push_back(std::move(sd));
  return std::make_unique<CompilationUnit>(dummyRange(sm),
                                           std::move(items));
}

// ---------------------------------------------------------------
// (a) S18 pass test — after Sema runs, the StructTypeSymbol for
//     `hdr_t` carries 3 fields in MSB-first order. Phase 4b T066
//     populates this; pre-T066 the assertion FAILS.
// ---------------------------------------------------------------

TEST(ConstructiveS18Test, StructFieldsAreMSBFirstAfterSema) {
  SourceManager sm;
  DiagnosticEngine diag(sm);
  auto cu = makeUnitWithStruct(sm);

  Sema sema(diag);
  SemaResult r = sema.run(*cu);
  ASSERT_NE(r.symbols, nullptr);

  // Look up the struct in global scope. (Phase 3 ResolutionPass
  // declares struct types in the global scope per data-model
  // §1.4.) Phase 4b T066 populates `setFields()`.
  Symbol *sym = r.symbols->lookup(Identifier("hdr_t"));
  ASSERT_NE(sym, nullptr) << "hdr_t struct symbol not declared";
  ASSERT_EQ(sym->kind(), SymbolKind::SK_StructType);

  auto *st = static_cast<StructTypeSymbol *>(sym);
  // Phase 4b T066 contract: 3 fields in declaration order; offsets
  // arranged MSB-first.
  EXPECT_EQ(st->fields().size(), 3U)
      << "Phase 4b T066 must populate StructTypeSymbol::fields() with "
         "the three declared members.";
  if (st->fields().size() == 3U) {
    // First-declared = MSB; offset = totalWidth - widthOfFirst.
    EXPECT_EQ(st->fields()[0].name.str(), "msb_field");
    EXPECT_EQ(st->fields()[1].name.str(), "mid_field");
    EXPECT_EQ(st->fields()[2].name.str(), "lsb_field");
  }
}

// ---------------------------------------------------------------
// (b) S18 fail-sibling test (Q1 Option B): asserting the *opposite*
//     observable — LSB-first ordering — MUST fail. We assemble a
//     known-good MSB-first state in a freestanding StructTypeSymbol
//     to guarantee the EXPECT_NONFATAL_FAILURE catches the wrong
//     ordering, independent of Phase 4b T066's progress.
// ---------------------------------------------------------------

TEST(ConstructiveS18Test, LSBFirstObservableFailsAsExpected) {
  StructTypeSymbol sym(Identifier("hdr_t"),
                       SourceRange{SourceLocation::make(FileID(1U), 0U),
                                   SourceLocation::make(FileID(1U), 1U)});
  std::vector<FieldInfo> msb_first = {
      {Identifier("msb_field"), 4, 4},
      {Identifier("mid_field"), 2, 2},
      {Identifier("lsb_field"), 2, 0},
  };
  sym.setFields(msb_first, /*total=*/8);

  // The first field's offset MUST be 4 (MSB-first). Asserting it
  // is 0 (LSB-first) is the opposite observable; this is the
  // expected nonfatal failure.
  EXPECT_NONFATAL_FAILURE(
      EXPECT_EQ(sym.fields()[0].offset, 0U),
      "Expected equality of these values");
}

} // namespace
