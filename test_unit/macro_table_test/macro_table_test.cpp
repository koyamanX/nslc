// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// test_unit/macro_table_test/macro_table_test.cpp
//
// TDD fixtures for the `MacroTable` data-model entity 11 per
// `specs/002-m1-lex-preprocess/data-model.md` and FR-039
// (insertion-ordered iteration determinism).
//
// Authored RED before `lib/Preprocess/MacroTable.cpp` exists; this
// suite is expected to FAIL TO LINK against the unchanged tree
// because `nsl-preprocess` ships only its M0 anchor TU at this
// point — the `nsl::MacroTable` symbols are not yet defined. That is
// the Constitution Principle VIII RED-state evidence (FR-036).
//
// Coverage (per tasks.md T051):
//   * Insertion-ordered iteration (FR-039 / research §4): adding
//     macros A, B, C iterates as A→B→C, never hash-ordered.
//   * `undef(name)` removes the entry while preserving the
//     insertion-order of the surviving entries.
//   * Redefinition replaces the existing entry's body and emits a
//     `note: previous definition was here` via the
//     `DiagnosticEngine`.
//   * Predefined `-D` macros are inserted BEFORE any source-defined
//     macro, in command-line order — guaranteeing "first definition
//     wins" determinism.
//
// **Public API contract assumed by these fixtures** (the parallel
// `nsl-frontend-impl` agent shapes `lib/Preprocess/MacroTable.cpp`
// to satisfy this; if the API differs at land time, the failing
// link/compile is what reveals the discrepancy and the agents
// negotiate the surface):
//
//   namespace nsl {
//   struct MacroEntry {
//     llvm::StringRef name;
//     std::string     body;             // unexpanded textual body per P10
//     SourceRange     defining_loc;     // for "previous definition" notes
//   };
//   class MacroTable {
//    public:
//     MacroTable();
//     void   define(llvm::StringRef name, std::string body, SourceRange loc,
//                   DiagnosticEngine& diag);
//     void   undef (llvm::StringRef name);
//     bool   contains(llvm::StringRef name) const;
//     const MacroEntry* lookup(llvm::StringRef name) const;
//     // Insertion-ordered iteration per FR-039.
//     using const_iterator = /* MapVector<StringRef,MacroEntry>::const_iterator
//     */; const_iterator begin() const; const_iterator end()   const; size_t
//     size() const;
//   };
//   } // namespace nsl

#include "nsl/Basic/Diagnostic.h"
#include "nsl/Basic/SourceLocation.h"
#include "nsl/Basic/SourceManager.h"
#include "nsl/Preprocess/MacroTable.h"

#include "llvm/ADT/StringRef.h"

#include "gtest/gtest.h"
#include <algorithm>
#include <string>
#include <vector>

using nsl::DiagnosticEngine;
using nsl::FileID;
using nsl::Severity;
using nsl::SourceLocation;
using nsl::SourceManager;
using nsl::SourceRange;
using nsl::preprocess::MacroTable;

namespace {

// Tiny helper: synthesize a SourceRange in a single-byte buffer so
// the `defining_loc` field has a valid value without the test
// having to mock a file.
SourceRange syntheticRange(SourceManager &sm, FileID f) {
  SourceLocation b = SourceLocation::make(f, 0);
  SourceLocation e = SourceLocation::make(f, 1);
  return {b, e};
}

// Convenience: build a fresh SourceManager + a single in-memory
// buffer, return its FileID.
FileID makeBuf(SourceManager &sm) {
  std::vector<char> bytes{'X', '\0'};
  return sm.addBufferInMemory("synthetic.nsl", std::move(bytes));
}

// =============================================================
// 1. Insertion-ordered iteration (FR-039 / research §4)
// =============================================================

TEST(MacroTableTest, InsertionOrderIsPreservedAcrossThreeAdds) {
  SourceManager sm;
  DiagnosticEngine diag(sm);
  FileID f = makeBuf(sm);
  MacroTable mt;

  mt.insert("A", "1", syntheticRange(sm, f));
  mt.insert("B", "2", syntheticRange(sm, f));
  mt.insert("C", "3", syntheticRange(sm, f));

  std::vector<std::string> seen;
  for (auto it = mt.begin(); it != mt.end(); ++it) {
    seen.emplace_back(it->first);
  }
  ASSERT_EQ(seen.size(), 3U);
  EXPECT_EQ(seen[0], "A");
  EXPECT_EQ(seen[1], "B");
  EXPECT_EQ(seen[2], "C");
}

TEST(MacroTableTest, InsertionOrderSurvivesPathologicalNames) {
  // Names chosen so that hash-keyed iteration would shuffle them
  // unpredictably (the canonical FR-039 trap). Insertion-ordered
  // iteration MUST report ZZZ first, AAA last.
  SourceManager sm;
  DiagnosticEngine diag(sm);
  FileID f = makeBuf(sm);
  MacroTable mt;

  mt.insert("ZZZ", "1", syntheticRange(sm, f));
  mt.insert("MID", "2", syntheticRange(sm, f));
  mt.insert("AAA", "3", syntheticRange(sm, f));

  std::vector<std::string> seen;
  for (auto it = mt.begin(); it != mt.end(); ++it) {
    seen.emplace_back(it->first);
  }
  ASSERT_EQ(seen.size(), 3U);
  EXPECT_EQ(seen[0], "ZZZ");
  EXPECT_EQ(seen[1], "MID");
  EXPECT_EQ(seen[2], "AAA");
}

// =============================================================
// 2. undef removes while preserving survivor order
// =============================================================

TEST(MacroTableTest, UndefRemovesEntry) {
  SourceManager sm;
  DiagnosticEngine diag(sm);
  FileID f = makeBuf(sm);
  MacroTable mt;

  mt.insert("A", "1", syntheticRange(sm, f));
  mt.insert("B", "2", syntheticRange(sm, f));
  EXPECT_TRUE(mt.defined("A"));
  EXPECT_TRUE(mt.defined("B"));

  mt.undef("A");
  EXPECT_FALSE(mt.defined("A"));
  EXPECT_TRUE(mt.defined("B"));
  EXPECT_EQ(mt.size(), 1U);
}

TEST(MacroTableTest, UndefPreservesSurvivorOrder) {
  SourceManager sm;
  DiagnosticEngine diag(sm);
  FileID f = makeBuf(sm);
  MacroTable mt;

  mt.insert("A", "1", syntheticRange(sm, f));
  mt.insert("B", "2", syntheticRange(sm, f));
  mt.insert("C", "3", syntheticRange(sm, f));
  mt.insert("D", "4", syntheticRange(sm, f));

  // Remove the middle entry; A → C → D order MUST hold.
  mt.undef("B");

  std::vector<std::string> seen;
  for (auto it = mt.begin(); it != mt.end(); ++it) {
    seen.emplace_back(it->first);
  }
  ASSERT_EQ(seen.size(), 3U);
  EXPECT_EQ(seen[0], "A");
  EXPECT_EQ(seen[1], "C");
  EXPECT_EQ(seen[2], "D");
}

TEST(MacroTableTest, UndefOfMissingNameIsNoOp) {
  SourceManager sm;
  DiagnosticEngine diag(sm);
  FileID f = makeBuf(sm);
  MacroTable mt;

  mt.insert("A", "1", syntheticRange(sm, f));
  mt.undef("NEVER_DEFINED");
  EXPECT_TRUE(mt.defined("A"));
  EXPECT_EQ(mt.size(), 1U);
}

// =============================================================
// 3. Redefinition replaces + emits "previous definition" note
// =============================================================

TEST(MacroTableTest, RedefinitionReplacesBody) {
  SourceManager sm;
  DiagnosticEngine diag(sm);
  FileID f = makeBuf(sm);
  MacroTable mt;

  mt.insert("X", "OLD_BODY", syntheticRange(sm, f));
  // Track B's API: insert() returns false if name exists; redefine()
  // is the explicit replace path.
  nsl::SourceRange prev;
  mt.redefine("X", "NEW_BODY", syntheticRange(sm, f), &prev);

  ASSERT_TRUE(mt.defined("X"));
  const auto *e = mt.lookup("X");
  ASSERT_NE(e, nullptr);
  EXPECT_EQ(e->body, "NEW_BODY");
}

TEST(MacroTableTest, RedefinitionEmitsPreviousDefinitionNote) {
  SourceManager sm;
  DiagnosticEngine diag(sm);
  FileID f = makeBuf(sm);
  MacroTable mt;

  mt.insert("X", "OLD", syntheticRange(sm, f));
  // Track B's API: insert() returns false if name exists (caller
  // decides redefine vs error). The redefine() path captures the
  // previous SourceRange so the caller can attach a `note: previous
  // definition was here` to its diagnostic.
  nsl::SourceRange previous_loc;
  mt.redefine("X", "NEW", syntheticRange(sm, f), &previous_loc);
  EXPECT_TRUE(previous_loc.isValid())
      << "redefine() must surface the previous SourceRange so the caller "
         "can attach a 'previous definition was here' note.";
}

TEST(MacroTableTest, RedefinitionPreservesInsertionOrder) {
  // Redefinition replaces the entry IN PLACE — its insertion-order
  // position does NOT change.
  SourceManager sm;
  DiagnosticEngine diag(sm);
  FileID f = makeBuf(sm);
  MacroTable mt;

  mt.insert("A", "1", syntheticRange(sm, f));
  mt.insert("B", "2", syntheticRange(sm, f));
  mt.insert("C", "3", syntheticRange(sm, f));
  // Redefine B; iteration order MUST stay A → B → C.
  nsl::SourceRange prev;
  mt.redefine("B", "2_NEW", syntheticRange(sm, f), &prev);

  std::vector<std::string> seen;
  for (auto it = mt.begin(); it != mt.end(); ++it) {
    seen.emplace_back(it->first);
  }
  ASSERT_EQ(seen.size(), 3U);
  EXPECT_EQ(seen[0], "A");
  EXPECT_EQ(seen[1], "B");
  EXPECT_EQ(seen[2], "C");
}

// =============================================================
// 4. Predefined `-D` macros land BEFORE source-defined macros, in
//    command-line order
// =============================================================
//
// FR-030 + the data-model entity 11 invariant: "Predefined macros
// (from `-D` flags) are inserted in command-line order *before* any
// source-defined macro; this gives `-D` macros priority for 'first
// definition wins' determinism."
//
// The fixture mimics the driver's flow: `-D NAME=value` flags are
// applied first (in flag order), THEN source `#define`s are
// processed.

TEST(MacroTableTest, PredefinedMacrosLandFirstInOrder) {
  SourceManager sm;
  DiagnosticEngine diag(sm);
  FileID f = makeBuf(sm);
  MacroTable mt;

  // Step 1: simulate driver applying -D flags in order.
  mt.insert("D1_FIRST", "1", syntheticRange(sm, f));
  mt.insert("D2_SECOND", "2", syntheticRange(sm, f));

  // Step 2: simulate source #define directives.
  mt.insert("SRC_FIRST", "X", syntheticRange(sm, f));
  mt.insert("SRC_SECOND", "Y", syntheticRange(sm, f));

  std::vector<std::string> seen;
  for (auto it = mt.begin(); it != mt.end(); ++it) {
    seen.emplace_back(it->first);
  }
  ASSERT_EQ(seen.size(), 4U);
  EXPECT_EQ(seen[0], "D1_FIRST");
  EXPECT_EQ(seen[1], "D2_SECOND");
  EXPECT_EQ(seen[2], "SRC_FIRST");
  EXPECT_EQ(seen[3], "SRC_SECOND");
}

// =============================================================
// 5. Lookup is exact-match (insurance against accidental prefix
//    matching)
// =============================================================

TEST(MacroTableTest, LookupRejectsPrefixOnlyMatch) {
  SourceManager sm;
  DiagnosticEngine diag(sm);
  FileID f = makeBuf(sm);
  MacroTable mt;

  mt.insert("FOO", "1", syntheticRange(sm, f));
  mt.insert("FOO_BAR", "2", syntheticRange(sm, f));

  EXPECT_NE(mt.lookup("FOO"), nullptr);
  EXPECT_NE(mt.lookup("FOO_BAR"), nullptr);
  EXPECT_EQ(mt.lookup("FOO_BAZ"), nullptr);
  EXPECT_EQ(mt.lookup("FO"), nullptr);
  EXPECT_EQ(mt.lookup(""), nullptr);
}

} // namespace
