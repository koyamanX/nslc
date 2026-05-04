// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// test_unit/recovery_set_test/recovery_set_test.cpp
//
// TDD fixtures (T054 of `specs/005-m2-parser/tasks.md`) for the
// **recovery primitives** that Track L will land in
// `lib/Parse/Recovery.h` (private header) + `lib/Parse/Recovery.cpp`
// (per task T056). The three primitives under test:
//
//   1. `nsl::parse::TokenSet` — a `constexpr` bitset over
//      `TokenKind` (research §3). Operations: `contains(TokenKind)`,
//      `with(TokenKind)` / `union_with(TokenSet)` for compile-time
//      composition.
//   2. `Parser::skipUntil(TokenSet)` — deterministic forward scan.
//      Advances the lexer cursor until `peek().kind() ∈ set` or EOF.
//      Returns the token kind it stopped on (so the caller can
//      decide whether to consume it).
//   3. `RecoveryGuard` — RAII helper that pushes a recovery scope on
//      `Parser`'s per-rule recovery-scope stack. The active set is
//      the **union** of all live guard sets — nested `parseFoo()`
//      calls inherit their parents' tokens (research §3 + the
//      contract's recovery-set table row "inherited from enclosing
//      item-list").
//
// **Specification anchors**:
//   - spec.md FR-019 (DiagnosticEngine), FR-020/FR-021 (recovery
//     framework + per-rule documented sets).
//   - parser-recovery.contract.md §"Recovery model" — pins the
//     `skipUntil(set)` semantics: "advances the lexer until either
//     `peek().kind() ∈ set` OR `peek().kind() == TokenKind::Eof`."
//   - parser-recovery.contract.md §"Forbidden behaviors": "Recovery
//     MUST NOT introduce non-determinism — the recovery set is a
//     `constexpr` bitset; the `skipUntil()` walk is a deterministic
//     forward scan (Principle V)."
//   - research.md §3 — the API shape these tests assume.
//   - /speckit-clarify Q1 (2026-04-27): full multi-error recovery
//     means the framework MUST exist at every grammar level; the
//     primitives below are the load-bearing surface that makes
//     that possible.
//
// **Failing-state evidence (Principle VIII NON-NEGOTIABLE; FR-029)**:
// against parent commit `39cc618`, NEITHER `lib/Parse/Recovery.h`
// NOR `lib/Parse/Recovery.cpp` exists. The `#include "Recovery.h"`
// below resolves to no header — the translation unit FAILS TO
// COMPILE. That observed failure IS the FR-029 evidence Track K
// commits ahead of Track L's implementation. Once Track L lands
// the header (T056), this binary builds and the assertions exercise
// the primitives.
//
// **API-shape coordination**: this test file is the proximal
// authority for the `TokenSet` / `skipUntil` / `RecoveryGuard` API
// shape. Track L's Recovery.h MUST conform — or, if Track L
// chooses a different shape, the integration patch reconciles the
// TWO files (this test + Recovery.h) in one commit. The shape
// asserted here mirrors research §3 verbatim.

#include "Recovery.h" // private header — Track L authors
#include "nsl/Basic/Diagnostic.h"
#include "nsl/Basic/SourceManager.h"
#include "nsl/Lex/Lexer.h"
#include "nsl/Lex/Token.h"

// `Parser` is the private impl class living in `lib/Parse/ParserImpl.h`.
// We reach into it directly (white-box) per the precedent set by
// `precedence_table_test.cpp` — the suite's `target_include_directories`
// already adds `lib/Parse/` to the include path.
#include "ParserImpl.h"

#include "gtest/gtest.h"
#include <vector>

using nsl::TokenKind;
using nsl::parse::Parser;
using nsl::parse::RecoveryGuard;
using nsl::parse::TokenSet;

namespace {

std::vector<char> bytesOf(const char *literal) {
  std::vector<char> out;
  while (*literal != 0) {
    out.push_back(*literal++);
  }
  return out;
}

// ---------------------------------------------------------------------------
// TokenSet — bitset operations
// ---------------------------------------------------------------------------

// (1) An empty `TokenSet` reports `contains()` false for every kind.
// Per research §3, default construction yields the empty set so a
// `RecoveryGuard` with no explicit tokens degenerates to "skipUntil
// EOF only" (the parser-recovery.contract.md §"Recovery model"
// case-2 unwind).
TEST(RecoverySet, EmptySetContainsNothing) {
  constexpr TokenSet empty{};
  EXPECT_FALSE(empty.contains(TokenKind::tk_semicolon));
  EXPECT_FALSE(empty.contains(TokenKind::tk_rbrace));
  EXPECT_FALSE(empty.contains(TokenKind::tk_module));
  EXPECT_FALSE(empty.contains(TokenKind::tk_eof));
}

// (2) `TokenSet` constructed from a brace-enclosed list contains
// exactly the listed kinds, nothing else.
TEST(RecoverySet, ConstructedFromListContainsExactlyThose) {
  constexpr TokenSet semi_or_rbrace =
      TokenSet({TokenKind::tk_semicolon, TokenKind::tk_rbrace});
  EXPECT_TRUE(semi_or_rbrace.contains(TokenKind::tk_semicolon));
  EXPECT_TRUE(semi_or_rbrace.contains(TokenKind::tk_rbrace));
  EXPECT_FALSE(semi_or_rbrace.contains(TokenKind::tk_module));
  EXPECT_FALSE(semi_or_rbrace.contains(TokenKind::tk_lbrace));
  EXPECT_FALSE(semi_or_rbrace.contains(TokenKind::tk_eof));
}

// (3) Union (`|`) of two sets contains every member of either side.
// Used by `RecoveryGuard` to compose nested-scope inherited sets.
TEST(RecoverySet, UnionContainsBothOperands) {
  constexpr TokenSet a = TokenSet({TokenKind::tk_semicolon});
  constexpr TokenSet b = TokenSet({TokenKind::tk_rbrace});
  constexpr TokenSet ab = a | b;
  EXPECT_TRUE(ab.contains(TokenKind::tk_semicolon));
  EXPECT_TRUE(ab.contains(TokenKind::tk_rbrace));
  EXPECT_FALSE(ab.contains(TokenKind::tk_module));
}

// (4) Top-level recovery set per the contract's recovery-set table
// row 1: `{struct, declare, module, param_int, param_str, Eof}`.
// Verifies the contract's enumerated set is expressible as a
// `constexpr` literal (research §3 — "constexpr bitset" requirement
// is load-bearing for Principle V determinism).
TEST(RecoverySet, TopLevelRecoverySetContainsContractTokens) {
  constexpr TokenSet top_level = TokenSet(
      {TokenKind::tk_struct_, TokenKind::tk_declare, TokenKind::tk_module,
       TokenKind::tk_param_int, TokenKind::tk_param_str, TokenKind::tk_eof});
  EXPECT_TRUE(top_level.contains(TokenKind::tk_struct_));
  EXPECT_TRUE(top_level.contains(TokenKind::tk_declare));
  EXPECT_TRUE(top_level.contains(TokenKind::tk_module));
  EXPECT_TRUE(top_level.contains(TokenKind::tk_param_int));
  EXPECT_TRUE(top_level.contains(TokenKind::tk_param_str));
  EXPECT_TRUE(top_level.contains(TokenKind::tk_eof));

  // Negative checks — tokens that are NOT in the top-level set
  // (they belong to per-item recovery sets, not the top-level).
  EXPECT_FALSE(top_level.contains(TokenKind::tk_semicolon))
      << "`;` is in module-item / declare-item / seq-item sets, "
         "NOT in the top-level set per the contract's row 1.";
  EXPECT_FALSE(top_level.contains(TokenKind::tk_func));
  EXPECT_FALSE(top_level.contains(TokenKind::tk_reg));
}

// ---------------------------------------------------------------------------
// Parser::skipUntil(TokenSet) — deterministic forward scan
// ---------------------------------------------------------------------------

// (5) `skipUntil` over a stream where the recovery token is the
// FIRST token: cursor MUST NOT advance (the caller's contract is
// "stop AT the recovery token, don't consume it").
//
// Source: `; reg q;` — the `;` is at position 0; skipUntil({Semi})
// must leave `peek()` at the `;`.
TEST(RecoverySet, SkipUntilStopsAtFirstMatchingToken) {
  nsl::SourceManager sm;
  nsl::FileID const fid =
      sm.addBufferInMemory("/virt/skip-stop-first.nsl", bytesOf("; reg q;\n"));
  ASSERT_TRUE(fid.isValid());
  nsl::DiagnosticEngine diag(sm);
  nsl::Lexer lex(sm, fid, diag);
  Parser p(lex, diag);

  ASSERT_EQ(p.peekKind(), TokenKind::tk_semicolon)
      << "Pre-skip: cursor must be at `;` (the first token).";

  TokenKind stopped =
      ::nsl::parse::skipUntil(p, TokenSet({TokenKind::tk_semicolon}));

  EXPECT_EQ(stopped, TokenKind::tk_semicolon);
  EXPECT_EQ(p.peekKind(), TokenKind::tk_semicolon)
      << "skipUntil MUST NOT consume the recovery token — the "
         "caller decides whether to consume (see contract §"
         "\"Resume semantics\" column).";
}

// (6) `skipUntil` advances forward through non-matching tokens and
// stops at the first member of the set it encounters.
//
// Source: `wire q ; reg r ;` — skipUntil({Semi}) starting at `wire`
// must advance through `wire`, `q`, then stop at the first `;`.
TEST(RecoverySet, SkipUntilAdvancesToFirstMatch) {
  nsl::SourceManager sm;
  nsl::FileID const fid = sm.addBufferInMemory("/virt/skip-advance.nsl",
                                               bytesOf("wire q ; reg r ;\n"));
  ASSERT_TRUE(fid.isValid());
  nsl::DiagnosticEngine diag(sm);
  nsl::Lexer lex(sm, fid, diag);
  Parser p(lex, diag);

  ASSERT_EQ(p.peekKind(), TokenKind::tk_wire);

  TokenKind stopped =
      ::nsl::parse::skipUntil(p, TokenSet({TokenKind::tk_semicolon}));

  EXPECT_EQ(stopped, TokenKind::tk_semicolon);
  EXPECT_EQ(p.peekKind(), TokenKind::tk_semicolon)
      << "Cursor must rest AT the first `;`, having skipped past "
         "`wire` and `q` in deterministic forward order.";
}

// (7) When NO member of the set appears in the remaining stream,
// `skipUntil` walks all the way to EOF and stops there.
// Per the contract: "case 2 — `peek().kind() == TokenKind::Eof`"
// is the EOF-unwind path.
//
// Source: `wire q wire r` (no `;`); skipUntil({Semi}) terminates
// at `tk_eof`.
TEST(RecoverySet, SkipUntilStopsAtEofWhenNoMatch) {
  nsl::SourceManager sm;
  nsl::FileID const fid =
      sm.addBufferInMemory("/virt/skip-eof.nsl", bytesOf("wire q wire r\n"));
  ASSERT_TRUE(fid.isValid());
  nsl::DiagnosticEngine diag(sm);
  nsl::Lexer lex(sm, fid, diag);
  Parser p(lex, diag);

  TokenKind stopped =
      ::nsl::parse::skipUntil(p, TokenSet({TokenKind::tk_semicolon}));

  EXPECT_EQ(stopped, TokenKind::tk_eof);
  EXPECT_EQ(p.peekKind(), TokenKind::tk_eof)
      << "EOF unwind: skipUntil MUST stop at EOF when no recovery "
         "token appears (parser-recovery.contract.md §\"Recovery "
         "model\" case 2).";
}

// (8) Determinism (Principle V): two `skipUntil` runs on the same
// input MUST advance the cursor the same number of tokens.
// We re-create the parser to reset the cursor and re-run; the
// stopped-at kind MUST be byte-identical.
TEST(RecoverySet, SkipUntilIsDeterministicAcrossRuns) {
  auto run = []() -> TokenKind {
    nsl::SourceManager sm;
    nsl::FileID const fid = sm.addBufferInMemory(
        "/virt/skip-determinism.nsl", bytesOf("module foo { wire q ; }\n"));
    nsl::DiagnosticEngine diag(sm);
    nsl::Lexer lex(sm, fid, diag);
    Parser p(lex, diag);
    return ::nsl::parse::skipUntil(p, TokenSet({TokenKind::tk_semicolon}));
  };

  TokenKind first = run();
  TokenKind second = run();
  EXPECT_EQ(first, second)
      << "Principle V: two skipUntil runs on identical input must "
         "produce identical stop tokens.";
  EXPECT_EQ(first, TokenKind::tk_semicolon);
}

// ---------------------------------------------------------------------------
// RecoveryGuard — push/pop nesting; inherited sets
// ---------------------------------------------------------------------------

// (9) An outer `RecoveryGuard` makes its set the active recovery
// set. `Parser::activeRecoverySet()` returns the union of all live
// guard sets; for one outer guard, that union equals the guard's
// own set.
TEST(RecoverySet, OuterGuardSetsActiveRecovery) {
  nsl::SourceManager sm;
  nsl::FileID const fid =
      sm.addBufferInMemory("/virt/guard-outer.nsl", bytesOf(""));
  nsl::DiagnosticEngine diag(sm);
  nsl::Lexer lex(sm, fid, diag);
  Parser p(lex, diag);

  // Pre-guard: the active set is empty (a top-level skipUntil with
  // no guard would skip to EOF unconditionally — the no-op safe
  // floor per the contract's "case 2 unwind").
  EXPECT_FALSE(p.currentRecoverySet().contains(TokenKind::tk_module));

  {
    RecoveryGuard outer(p, TokenSet({TokenKind::tk_module}));
    EXPECT_TRUE(p.currentRecoverySet().contains(TokenKind::tk_module));
  }

  // Pop on guard destruction: the module token is gone again.
  EXPECT_FALSE(p.currentRecoverySet().contains(TokenKind::tk_module));
}

// (10) Nested guards UNION their sets — the contract's "inherited
// from enclosing item-list" semantics. After pushing `{Semi}` over
// `{Module}`, the active set contains BOTH; on inner-guard pop, the
// outer's `{Module}` remains.
TEST(RecoverySet, NestedGuardsUnionSets) {
  nsl::SourceManager sm;
  nsl::FileID const fid =
      sm.addBufferInMemory("/virt/guard-nested.nsl", bytesOf(""));
  nsl::DiagnosticEngine diag(sm);
  nsl::Lexer lex(sm, fid, diag);
  Parser p(lex, diag);

  RecoveryGuard outer(p, TokenSet({TokenKind::tk_module}));
  EXPECT_TRUE(p.currentRecoverySet().contains(TokenKind::tk_module));
  EXPECT_FALSE(p.currentRecoverySet().contains(TokenKind::tk_semicolon));

  {
    RecoveryGuard inner(p, TokenSet({TokenKind::tk_semicolon}));
    // Both tokens active inside the inner scope.
    EXPECT_TRUE(p.currentRecoverySet().contains(TokenKind::tk_module))
        << "Outer guard's tokens MUST remain active inside an inner "
           "guard (research §3: inherited recovery sets).";
    EXPECT_TRUE(p.currentRecoverySet().contains(TokenKind::tk_semicolon));
  }

  // After inner pop: outer's `{Module}` survives; inner's `{Semi}`
  // is gone.
  EXPECT_TRUE(p.currentRecoverySet().contains(TokenKind::tk_module));
  EXPECT_FALSE(p.currentRecoverySet().contains(TokenKind::tk_semicolon))
      << "Inner guard's tokens MUST NOT leak past its scope "
         "(stack-discipline; FR-021 deterministic recovery).";
}

// (11) skipUntil() with the `RecoveryGuard`-active set: convenience
// accessor `Parser::skipUntilActiveRecovery()` performs
// `skipUntil(activeRecoverySet())`. This is the call shape every
// `parseFoo()` uses on the error path.
//
// Source: `wire q ; reg r ;` under a guard whose set is `{Semi}`.
// `skipUntilActiveRecovery()` resumes at the first `;`.
TEST(RecoverySet, SkipUntilActiveRecoveryUsesGuardSet) {
  nsl::SourceManager sm;
  nsl::FileID const fid = sm.addBufferInMemory("/virt/guard-skip.nsl",
                                               bytesOf("wire q ; reg r ;\n"));
  nsl::DiagnosticEngine diag(sm);
  nsl::Lexer lex(sm, fid, diag);
  Parser p(lex, diag);

  RecoveryGuard guard(p, TokenSet({TokenKind::tk_semicolon}));
  TokenKind stopped = ::nsl::parse::skipUntil(p, p.currentRecoverySet());
  EXPECT_EQ(stopped, TokenKind::tk_semicolon);
  EXPECT_EQ(p.peekKind(), TokenKind::tk_semicolon);
}

} // namespace
