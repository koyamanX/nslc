// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// lib/Parse/PrecedenceTable.h — private to `nsl-parse`. Pratt-
// precedence table consumed only by `lib/Parse/ParseExpr.cpp` (M2
// Phase 3). The table encodes the operator inventory from
// `docs/spec/nsl_lang.ebnf §11` in the form Pratt parsing wants:
// per-token `nud` (null denotation; prefix or leaf-start dispatch)
// and `led` (left denotation; infix continuation dispatch) entries
// with associated precedence levels.
//
// Why a header-only `static constexpr` table:
//   - Determinism (FR-030; Principle V): a `constexpr` table has no
//     environment dependency. Two builds produce identical parser
//     behavior given identical input bytes.
//   - Single source of truth: adding a new operator requires
//     editing one row in `kPrecedenceTable[]`; the parser's loop
//     in `ParseExpr.cpp` is operator-agnostic.
//   - Test-friendly: the table is a public-to-`nsl-parse` constant,
//     so `test_unit/recovery_set_test/precedence_table_test.cpp`
//     (Track B / Phase 2 T017) can include it directly.
//
// The Pratt mechanics are textbook (Pratt 1973 / Crockford 2007):
// `parseExpr(precFloor)` first invokes the current token's `nud`
// (if any) to consume a prefix or leaf; then loops, while the
// current token has a `led` entry whose precedence ≥ `precFloor`,
// consuming the operator and recursively parsing the RHS at the
// operator's precedence (or precedence + 1 for left-assoc).
//
// Precedence levels are integers; higher binds tighter. The exact
// numerical values are arbitrary as long as the relative order
// matches `lang.ebnf §11` (and the per-N2 prefix-`&|^` is below
// no level since prefixes have their own dispatch). The values
// chosen below leave gaps for inserting future operators without
// renumbering every row (a routine maintenance hedge).

#ifndef NSL_PARSE_PRECEDENCETABLE_H
#define NSL_PARSE_PRECEDENCETABLE_H

#include "nsl/Lex/Token.h"

#include <array>
#include <cstdint>

namespace nsl::parse {

/// Numeric precedence level. Higher = binds tighter. Level 0 is the
/// "no `led`" sentinel — Pratt-loop terminator.
enum class PrecLevel : uint8_t {
  None = 0,             ///< sentinel: no `led`; operator does not chain
  LogicalOr = 10,       ///< `||`
  LogicalAnd = 20,      ///< `&&`
  BitOr = 30,           ///< `|`
  BitXor = 40,          ///< `^`
  BitAnd = 50,          ///< `&`
  Equality = 60,        ///< `==`, `!=`
  Relational = 70,      ///< `<`, `<=`, `>`, `>=`
  Shift = 80,           ///< `<<`, `>>`
  Additive = 90,        ///< `+`, `-`
  Multiplicative = 100, ///< `*`, `/`, `%`
  Postfix = 110,        ///< postfix `++`, `--` (binds tighter than `*`,
                        ///<   `/`, `%`; matches C/C++ convention so
                        ///<   `a * i++` parses as `a * (i++)`)
  Conditional = 5,      ///< `?:` ternary (right-assoc; below logical-or)
};

/// Associativity for `led` entries. Pratt: left-assoc → recurse with
/// `prec + 1` as the floor; right-assoc → recurse with `prec` itself.
enum class Assoc : uint8_t { None, Left, Right };

/// One row of the Pratt table. Indexed by `TokenKind`.
///
/// Per-N2 prefix-vs-infix: `&` `|` `^` carry both `hasNud` (true,
/// reduction operator) and a non-`None` `ledPrec` (binary bitwise);
/// the parser dispatches based on whether a left operand is on the
/// expression stack at decision time.
struct PrecEntry {
  /// True iff `<TokenKind>` may begin an expression (prefix /
  /// leaf-start). The actual `nud`-handler dispatch lives in
  /// `ParseExpr.cpp` — this flag only signals "ParseExpr should
  /// route the token to a prefix/leaf path."
  bool hasNud;

  /// Precedence of the `<TokenKind>` as an infix operator.
  /// `PrecLevel::None` means "this token is not infix; the
  /// `parseExpr` loop terminates when it sees this token in the
  /// peek slot."
  PrecLevel ledPrec;

  /// Associativity of the infix form. Ignored when
  /// `ledPrec == PrecLevel::None`.
  Assoc ledAssoc;
};

/// Per-token Pratt table. Source order matches `TokenKind`
/// declaration order from `include/nsl/Lex/Token.h` — every
/// `tk_count` entry is initialized via designated form so the
/// compiler catches order drift if `Token.h` shifts.
///
/// Designated initializers are C99 — accepted by C++17 (since C++20
/// they're official). Default-initialized entries have
/// `{false, PrecLevel::None, Assoc::None}` — i.e., neither nud nor
/// led, which is what we want for sentinels, identifiers (the
/// parser dispatches them through a separate identifier-leaf
/// path), keywords (most are not expression-starters at all), etc.
///
/// `static constexpr` makes the table a compile-time literal — zero
/// runtime initialization, available from any consumer that
/// `#include`s this header. Inline since C++17 makes the
/// definition uniqued across TUs.
inline constexpr std::array<PrecEntry, static_cast<size_t>(TokenKind::tk_count)>
buildPrecedenceTable() {
  std::array<PrecEntry, static_cast<size_t>(TokenKind::tk_count)> t{};
  // Default rows are `{false, None, None}` (zero-init).

  // ----- Leaves: literals + identifier-like ----- //
  t[static_cast<size_t>(TokenKind::tk_identifier)] = {true, PrecLevel::None,
                                                      Assoc::None};
  t[static_cast<size_t>(TokenKind::tk_string_lit)] = {true, PrecLevel::None,
                                                      Assoc::None};
  t[static_cast<size_t>(TokenKind::tk_decimal_lit)] = {true, PrecLevel::None,
                                                       Assoc::None};
  t[static_cast<size_t>(TokenKind::tk_hex_lit)] = {true, PrecLevel::None,
                                                   Assoc::None};
  t[static_cast<size_t>(TokenKind::tk_binary_lit)] = {true, PrecLevel::None,
                                                      Assoc::None};
  t[static_cast<size_t>(TokenKind::tk_octal_lit)] = {true, PrecLevel::None,
                                                     Assoc::None};
  t[static_cast<size_t>(TokenKind::tk_system_function)] = {
      true, PrecLevel::None, Assoc::None};

  // ----- Parenthesized expression / concat / repeat starts ----- //
  t[static_cast<size_t>(TokenKind::tk_lparen)] = {true, PrecLevel::None,
                                                  Assoc::None};
  t[static_cast<size_t>(TokenKind::tk_lbrace)] = {true, PrecLevel::None,
                                                  Assoc::None};
  t[static_cast<size_t>(TokenKind::tk_dot_lbrace)] = {
      true, PrecLevel::None, Assoc::None}; // N3 `.{` LHS-concat

  // ----- Prefix unary operators ----- //
  t[static_cast<size_t>(TokenKind::tk_plus)] = {true, PrecLevel::Additive,
                                                Assoc::Left};
  t[static_cast<size_t>(TokenKind::tk_minus)] = {true, PrecLevel::Additive,
                                                 Assoc::Left};
  t[static_cast<size_t>(TokenKind::tk_tilde)] = {true, PrecLevel::None,
                                                 Assoc::None};
  t[static_cast<size_t>(TokenKind::tk_logical_not)] = {true, PrecLevel::None,
                                                       Assoc::None};

  // ----- Inc/Dec operators (`lang.ebnf §11` lines 654–657) ----- //
  // `++`/`--` are listed under `primary_expr` in both prefix
  // (`++ identifier`) and postfix (`identifier ++`) forms. The prefix
  // form is dispatched as a nud in `parseNudExpr`; the postfix form
  // rides the Pratt led-table at `Postfix` precedence — tighter than
  // `Multiplicative` so `a * i++` parses as `a * (i++)`, matching
  // C/C++ convention. The Pratt loop's per-token branch in
  // `ParseExpr.cpp` constructs `IncDecExpr{prefix=false}` directly;
  // `Assoc::Left` is a placeholder (postfix has no second operand).
  t[static_cast<size_t>(TokenKind::tk_plus_plus)] = {true, PrecLevel::Postfix,
                                                     Assoc::Left};
  t[static_cast<size_t>(TokenKind::tk_minus_minus)] = {true, PrecLevel::Postfix,
                                                       Assoc::Left};

  // ----- N2: `&` `|` `^` are BOTH prefix (reduction) AND infix
  //       (bitwise binary). The parser inspects the call site to
  //       pick the right denotation.
  t[static_cast<size_t>(TokenKind::tk_amp)] = {true, PrecLevel::BitAnd,
                                               Assoc::Left};
  t[static_cast<size_t>(TokenKind::tk_pipe)] = {true, PrecLevel::BitOr,
                                                Assoc::Left};
  t[static_cast<size_t>(TokenKind::tk_caret)] = {true, PrecLevel::BitXor,
                                                 Assoc::Left};

  // ----- N5: `#` sign-extend in expression position (after the
  //       width literal); the lexer emits `tk_hash_sign_extend`
  //       for the EXPRESSION-position case (the `#line` form is
  //       consumed at the M1/M2 seam and never reaches us). The
  //       sign-extend operator is binary in NSL: `width # value`.
  //       Modeling it as infix simplifies Pratt; precedence is
  //       Multiplicative-tight per the EBNF.
  t[static_cast<size_t>(TokenKind::tk_hash_sign_extend)] = {
      false, PrecLevel::Multiplicative, Assoc::Left};
  // Zero-extend `'` mirrors sign-extend in tightness.
  t[static_cast<size_t>(TokenKind::tk_apostrophe_zero_extend)] = {
      false, PrecLevel::Multiplicative, Assoc::Left};

  // ----- Multiplicative ----- //
  t[static_cast<size_t>(TokenKind::tk_star)] = {
      false, PrecLevel::Multiplicative, Assoc::Left};
  t[static_cast<size_t>(TokenKind::tk_slash)] = {
      false, PrecLevel::Multiplicative, Assoc::Left};
  t[static_cast<size_t>(TokenKind::tk_percent)] = {
      false, PrecLevel::Multiplicative, Assoc::Left};

  // ----- Shift ----- //
  t[static_cast<size_t>(TokenKind::tk_shift_left)] = {false, PrecLevel::Shift,
                                                      Assoc::Left};
  t[static_cast<size_t>(TokenKind::tk_shift_right)] = {false, PrecLevel::Shift,
                                                       Assoc::Left};

  // ----- Relational ----- //
  t[static_cast<size_t>(TokenKind::tk_less)] = {false, PrecLevel::Relational,
                                                Assoc::Left};
  t[static_cast<size_t>(TokenKind::tk_less_equal)] = {
      false, PrecLevel::Relational, Assoc::Left};
  t[static_cast<size_t>(TokenKind::tk_greater)] = {false, PrecLevel::Relational,
                                                   Assoc::Left};
  t[static_cast<size_t>(TokenKind::tk_greater_equal)] = {
      false, PrecLevel::Relational, Assoc::Left};

  // ----- Equality ----- //
  t[static_cast<size_t>(TokenKind::tk_equal)] = {false, PrecLevel::Equality,
                                                 Assoc::Left};
  t[static_cast<size_t>(TokenKind::tk_not_equal)] = {false, PrecLevel::Equality,
                                                     Assoc::Left};

  // ----- Logical ----- //
  t[static_cast<size_t>(TokenKind::tk_logical_and)] = {
      false, PrecLevel::LogicalAnd, Assoc::Left};
  t[static_cast<size_t>(TokenKind::tk_logical_or)] = {
      false, PrecLevel::LogicalOr, Assoc::Left};

  // ----- Conditional `?:` (right-assoc; lowest-binding) ----- //
  t[static_cast<size_t>(TokenKind::tk_question)] = {
      false, PrecLevel::Conditional, Assoc::Right};

  // ----- N1: `if (...) a else b` expression form. The parser
  //       routes `tk_if_` as a `nud` (prefix) that builds a
  //       `ConditionalExpr`. No `led` for `if`.
  t[static_cast<size_t>(TokenKind::tk_if_)] = {true, PrecLevel::None,
                                               Assoc::None};

  // ----- N11(b): no-parens `_random` / `_time` are leaves. The
  //       lexer emits `tk_system_function` for them already (see
  //       Token.h header comment). Already covered above.

  return t;
}

inline constexpr auto kPrecedenceTable = buildPrecedenceTable();

/// Convenience accessors. Both are noexcept and `O(1)`.
inline constexpr PrecEntry getPrecedence(TokenKind k) noexcept {
  // Deliberately permissive on out-of-range — callers should be
  // checking `kind() != tk_unknown` already; if not, returning a
  // zeroed entry yields a graceful "no nud, no led" result.
  auto idx = static_cast<size_t>(k);
  if (idx >= kPrecedenceTable.size()) {
    return PrecEntry{false, PrecLevel::None, Assoc::None};
  }
  return kPrecedenceTable[idx];
}

/// True iff `k` may begin an expression (prefix / leaf).
inline constexpr bool isExpressionStart(TokenKind k) noexcept {
  return getPrecedence(k).hasNud;
}

/// True iff `k` is an infix operator at any precedence level.
inline constexpr bool isInfixOperator(TokenKind k) noexcept {
  return getPrecedence(k).ledPrec != PrecLevel::None;
}

} // namespace nsl::parse

#endif // NSL_PARSE_PRECEDENCETABLE_H
