// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// test_unit/recovery_set_test/precedence_table_test.cpp
//
// TDD fixtures (M2 Phase 2, T017) for the Pratt-style operator
// precedence table introduced by `specs/005-m2-parser/`.
//
// **Specification anchors**:
//   - research §2 (`specs/005-m2-parser/research.md`): Pratt parser
//     for `lang.ebnf §11` expressions; `static constexpr` table
//     indexed by `TokenKind`; `nud` (null-denotation) entries for
//     prefix operators; `led` (left-denotation) entries for infix.
//     "N2's reduction-vs-bitwise (`&x` unary vs `a & b` binary) is
//     exactly Pratt's prefix-vs-infix dispatch — `&` has both a
//     `nud` (reduction operator) and a `led` (left denotation =
//     bitwise binary) entry".
//   - `lang.ebnf §11` (lines 595-705 of `docs/spec/nsl_lang.ebnf`)
//     pins the binary-operator precedence ladder verbatim:
//       conditional_expression  (lowest binary, ternary)
//       logical_or_expr   ||
//       logical_and_expr  &&
//       bitwise_or_expr   |
//       bitwise_xor_expr  ^
//       bitwise_and_expr  &
//       equality_expr     == !=
//       relational_expr   < <= > >=
//       shift_expr        << >>
//       additive_expr     + -
//       multiplicative_expr *  (highest binary)
//     Comment line 596: "low index = high precedence" — i.e., the
//     grammar's outer rule is the lowest binary precedence.
//
// **TDD evidence (Principle VIII NON-NEGOTIABLE)**: this file is
// authored before `lib/Parse/PrecedenceTable.h` exists. Against
// the unchanged tree at HEAD `e060eeb` `lib/Parse/CMakeLists.txt`
// is empty (only `add_nsl_library(nsl-parse ...)` skeleton), so
// the `#include "PrecedenceTable.h"` below resolves to no header
// → the translation unit fails to compile.
//
// **API surface assumed (research §2 + tasks.md T016)**:
//   - `lib/Parse/PrecedenceTable.h` is *private* to `nsl-parse`
//     (consumed only by `lib/Parse/ParseExpr.cpp`). For test
//     purposes the test target adds `lib/Parse/` to its include
//     path — the M0 `add_nsl_library_test` precedent allows this
//     for white-box tests of private headers.
//   - `prec(TokenKind)` returns an integral
//     precedence level. Higher numbers bind tighter (Pratt
//     convention; opposite of the EBNF's "low index = high
//     precedence" comment which describes the grammar nesting,
//     not the table). We assert RELATIVE ordering rather than
//     absolute integer values to remain robust to Track A's
//     specific encoding (any monotonic mapping from EBNF level to
//     integer satisfies the contract).
//   - `hasNud(TokenKind)` / `hasLed(TokenKind)`
//     report whether each denotation is present in the table. For
//     `&`, `|`, `^` BOTH must be true (research §2 N2 dispatch).

#include "PrecedenceTable.h"

#include "nsl/Lex/Token.h"
#include "PrecedenceTable.h"

#include "gtest/gtest.h"

using nsl::TokenKind;

namespace {

// ---- Test-local helpers bridging Track A's `PrecEntry`-struct API ------
//
// Track A's `lib/Parse/PrecedenceTable.h` exposes `getPrecedence()`
// returning a `PrecEntry` aggregate (`{hasNud, ledPrec, ledAssoc}`).
// These tests originally assumed three separate free functions
// (`getPrecedence` returning a scalar, plus `hasNud`/`hasLed` bools).
// The wrappers below project the struct to the assumed scalar/bool
// API so the assertion call sites stay readable. No public API
// drift — only test-local glue. Adjust here if Track A renames the
// fields.

inline ::nsl::parse::PrecLevel prec(::nsl::TokenKind k) noexcept {
  return ::nsl::parse::getPrecedence(k).ledPrec;
}

inline bool hasNud(::nsl::TokenKind k) noexcept {
  return ::nsl::parse::getPrecedence(k).hasNud;
}

inline bool hasLed(::nsl::TokenKind k) noexcept {
  return ::nsl::parse::getPrecedence(k).ledPrec != ::nsl::parse::PrecLevel::None;
}

// ---- Binary precedence ladder per `lang.ebnf §11` ----------------------

// Multiplicative (`*`) MUST bind strictly tighter than additive (`+`).
TEST(PrecedenceTable, MultiplicativeBindsTighterThanAdditive) {
  EXPECT_GT(prec(TokenKind::tk_star),
            prec(TokenKind::tk_plus))
      << "* MUST have higher Pratt precedence than +";
  EXPECT_GT(prec(TokenKind::tk_star),
            prec(TokenKind::tk_minus))
      << "* MUST have higher Pratt precedence than - (binary)";
}

// Additive (`+`/`-`) MUST bind strictly tighter than shift (`<<`/`>>`).
TEST(PrecedenceTable, AdditiveBindsTighterThanShift) {
  EXPECT_GT(prec(TokenKind::tk_plus),
            prec(TokenKind::tk_shift_left));
  EXPECT_GT(prec(TokenKind::tk_minus),
            prec(TokenKind::tk_shift_right));
  EXPECT_EQ(prec(TokenKind::tk_plus),
            prec(TokenKind::tk_minus))
      << "+ and - share an additive precedence level per §11";
}

// Shift (`<<`/`>>`) MUST bind strictly tighter than relational.
TEST(PrecedenceTable, ShiftBindsTighterThanRelational) {
  EXPECT_GT(prec(TokenKind::tk_shift_left),
            prec(TokenKind::tk_less));
  EXPECT_GT(prec(TokenKind::tk_shift_right),
            prec(TokenKind::tk_greater));
  EXPECT_EQ(prec(TokenKind::tk_shift_left),
            prec(TokenKind::tk_shift_right));
}

// Relational (`<`,`<=`,`>`,`>=`) MUST bind tighter than equality.
TEST(PrecedenceTable, RelationalBindsTighterThanEquality) {
  EXPECT_GT(prec(TokenKind::tk_less),
            prec(TokenKind::tk_equal));
  EXPECT_GT(prec(TokenKind::tk_greater),
            prec(TokenKind::tk_not_equal));
  EXPECT_EQ(prec(TokenKind::tk_less),
            prec(TokenKind::tk_less_equal));
  EXPECT_EQ(prec(TokenKind::tk_less),
            prec(TokenKind::tk_greater));
  EXPECT_EQ(prec(TokenKind::tk_less),
            prec(TokenKind::tk_greater_equal));
}

// Equality MUST bind tighter than bitwise-AND.
TEST(PrecedenceTable, EqualityBindsTighterThanBitwiseAnd) {
  EXPECT_GT(prec(TokenKind::tk_equal),
            prec(TokenKind::tk_amp));
  EXPECT_EQ(prec(TokenKind::tk_equal),
            prec(TokenKind::tk_not_equal));
}

// Bitwise-AND > Bitwise-XOR > Bitwise-OR (per `lang.ebnf §11`
// nesting `bitwise_and_expr < bitwise_xor_expr < bitwise_or_expr`).
TEST(PrecedenceTable, BitwiseAndXorOrLadder) {
  EXPECT_GT(prec(TokenKind::tk_amp),
            prec(TokenKind::tk_caret))
      << "& MUST bind tighter than ^ (bitwise_and_expr nests inside "
         "bitwise_xor_expr in §11)";
  EXPECT_GT(prec(TokenKind::tk_caret),
            prec(TokenKind::tk_pipe))
      << "^ MUST bind tighter than | (bitwise_xor_expr nests inside "
         "bitwise_or_expr in §11)";
}

// Bitwise-OR > Logical-AND > Logical-OR.
TEST(PrecedenceTable, BitwiseOrTighterThanLogical) {
  EXPECT_GT(prec(TokenKind::tk_pipe),
            prec(TokenKind::tk_logical_and));
  EXPECT_GT(prec(TokenKind::tk_logical_and),
            prec(TokenKind::tk_logical_or));
}

// ---- Nud / Led dispatch per N2 (research §2) ---------------------------

// `&` has BOTH a `nud` (reduction-AND, prefix per N2) and a `led`
// (bitwise-AND, infix). This is the load-bearing parser-note-N2
// disambiguation: at expression position with no left operand on
// the stack, the Pratt loop calls `nud(tk_amp)` → `UnaryExpr{
// op=ReduceAnd}`; with a left operand, `led(tk_amp)` →
// `BinaryExpr{op=BitAnd}`.
TEST(PrecedenceTable, AmpersandHasBothDenotations) {
  EXPECT_TRUE(hasNud(TokenKind::tk_amp))
      << "& MUST have a nud (reduction-AND) per N2";
  EXPECT_TRUE(hasLed(TokenKind::tk_amp))
      << "& MUST have a led (bitwise-AND) per §11 bitwise_and_expr";
}

TEST(PrecedenceTable, PipeHasBothDenotations) {
  EXPECT_TRUE(hasNud(TokenKind::tk_pipe))
      << "| MUST have a nud (reduction-OR) per N2";
  EXPECT_TRUE(hasLed(TokenKind::tk_pipe))
      << "| MUST have a led (bitwise-OR) per §11 bitwise_or_expr";
}

TEST(PrecedenceTable, CaretHasBothDenotations) {
  EXPECT_TRUE(hasNud(TokenKind::tk_caret))
      << "^ MUST have a nud (reduction-XOR) per N2";
  EXPECT_TRUE(hasLed(TokenKind::tk_caret))
      << "^ MUST have a led (bitwise-XOR) per §11 bitwise_xor_expr";
}

// `+`/`-` are also dual-denotation: nud (unary plus/minus per
// `unary_expr`) and led (additive). The dispatch is by stack
// state, not token kind, so both entries must populate.
TEST(PrecedenceTable, PlusAndMinusHaveBothDenotations) {
  EXPECT_TRUE(hasNud(TokenKind::tk_plus))
      << "+ MUST have a nud (unary plus per `unary_expr`)";
  EXPECT_TRUE(hasLed(TokenKind::tk_plus))
      << "+ MUST have a led (additive)";
  EXPECT_TRUE(hasNud(TokenKind::tk_minus))
      << "- MUST have a nud (unary minus per `unary_expr`)";
  EXPECT_TRUE(hasLed(TokenKind::tk_minus))
      << "- MUST have a led (additive)";
}

// `~` and `!` are ONLY prefix (no infix denotation in §11).
TEST(PrecedenceTable, PrefixOnlyOperatorsHaveOnlyNud) {
  EXPECT_TRUE(hasNud(TokenKind::tk_tilde))
      << "~ MUST have a nud (bitwise NOT per `unary_expr`)";
  EXPECT_FALSE(hasLed(TokenKind::tk_tilde))
      << "~ MUST NOT have a led — there is no infix `~` in §11";

  EXPECT_TRUE(hasNud(TokenKind::tk_logical_not))
      << "! MUST have a nud (logical NOT per `unary_expr`)";
  EXPECT_FALSE(hasLed(TokenKind::tk_logical_not))
      << "! MUST NOT have a led — there is no infix `!` in §11";
}

// `*` is ONLY infix (multiplicative). It is NOT a Pratt prefix
// operator in §11 (there is no `unary_expr` alternative beginning
// with `*` — pointer-deref is not in the NSL grammar).
TEST(PrecedenceTable, StarHasOnlyLed) {
  EXPECT_TRUE(hasLed(TokenKind::tk_star))
      << "* MUST have a led (multiplicative)";
  EXPECT_FALSE(hasNud(TokenKind::tk_star))
      << "* MUST NOT have a nud — no unary `*` in §11";
}

// `#` (sign-extend per N5) is INFIX in NSL's grammar:
//   `lang.ebnf §11` line 702:
//     sign_extend_expression = constant_expression "#" primary_expr
// So `#` carries a `led` entry with the width on its LEFT and the
// operand on its right. There is NO `nud` for `#` (it never appears
// in prefix position).
//
// Note: research §2 last bullet originally characterized `#` as a
// "prefix operator" — that wording is imprecise relative to the EBNF
// and is being corrected in the same change as this test. Track A's
// `PrecedenceTable.h` lines 155–164 implements the EBNF-correct
// model. (Principle VII spec/design coupling fix.)
TEST(PrecedenceTable, HashSignExtendIsInfix) {
  EXPECT_FALSE(hasNud(TokenKind::tk_hash_sign_extend))
      << "# is infix per EBNF §11 (`constant_expression # primary_expr`), "
         "not prefix";
  EXPECT_TRUE(hasLed(TokenKind::tk_hash_sign_extend))
      << "# MUST have a led — binary sign-extend per §11 line 702";
}

// ---- Negative cases ----------------------------------------------------

// Punctuation that is NEVER an operator MUST report no denotation
// and the *lowest* possible precedence (so a Pratt loop stops at
// `;` / `,` / `)` / `}` etc., never folding them into an expression).
TEST(PrecedenceTable, NonOperatorPunctuationHasNoDenotation) {
  EXPECT_FALSE(hasNud(TokenKind::tk_semicolon));
  EXPECT_FALSE(hasLed(TokenKind::tk_semicolon));
  EXPECT_FALSE(hasNud(TokenKind::tk_comma));
  EXPECT_FALSE(hasLed(TokenKind::tk_comma));
  EXPECT_FALSE(hasNud(TokenKind::tk_rparen));
  EXPECT_FALSE(hasLed(TokenKind::tk_rparen));
  EXPECT_FALSE(hasNud(TokenKind::tk_rbrace));
  EXPECT_FALSE(hasLed(TokenKind::tk_rbrace));
}

// `tk_eof` is a hard floor: any Pratt loop with `precFloor=0` MUST
// terminate at EOF. We assert `hasLed(tk_eof) == false` so the loop
// in `parseExpr()` never tries to fold EOF as an infix operator.
TEST(PrecedenceTable, EofHasNoDenotation) {
  EXPECT_FALSE(hasNud(TokenKind::tk_eof));
  EXPECT_FALSE(hasLed(TokenKind::tk_eof));
}

} // namespace
