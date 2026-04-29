// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// lib/Parse/Recovery.h — PRIVATE recovery primitives for `nsl-parse`.
//
// Per `specs/005-m2-parser/contracts/parser-recovery.contract.md`:
// the parser uses per-rule, constexpr-friendly `TokenSet` recovery
// tables plus a `RecoveryGuard` RAII that pushes/pops the active set
// on a per-rule stack (Parser::recovery_stack_). On a syntax error,
// the rule emits a diagnostic via `DiagnosticEngine`, then calls
// `skipUntil()` with the merged active set; once the lexer is parked
// at a token in the set (or EOF) the rule unwinds. The caller's loop
// inspects the parked token and either consumes a separator (`;`)
// and continues, or exits at `}` / EOF.
//
// All members here are `constexpr`-compatible where called from
// constexpr static-storage initializers in the per-`parseFoo()` sites
// (research §3 / FR-030 determinism: the per-rule recovery sets are
// resolved at compile time so the bitset never varies between runs).
//
// Layer: this header is consumed by `Parser.cpp` / `ParseDecl.cpp` /
// `ParseStmt.cpp` / `ParseExpr.cpp`. It is NOT exposed via
// `include/nsl/Parse/`. Adding members is a private refactor.

#ifndef NSL_LIB_PARSE_RECOVERY_H
#define NSL_LIB_PARSE_RECOVERY_H

#include "nsl/Lex/Token.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <initializer_list>

namespace nsl::parse {

class Parser;

/// Constexpr-friendly bitset over `nsl::TokenKind`. Size is computed
/// from `tk_count` (the trailing sentinel in `TokenKind`); each bit
/// is `1` iff the corresponding kind is a member of the set.
///
/// All operations are `constexpr` so per-rule recovery tables can be
/// declared as `static constexpr TokenSet` at function scope without
/// dragging the table into runtime initialization order (Principle V
/// determinism: byte-identical artifacts).
class TokenSet {
public:
  static constexpr std::size_t kBits =
      static_cast<std::size_t>(TokenKind::tk_count);
  static constexpr std::size_t kWords = (kBits + 63U) / 64U;

  constexpr TokenSet() noexcept : words_{} {}

  /// Build a set from an initializer list of token kinds. Used at the
  /// declaration sites of per-rule recovery tables.
  constexpr TokenSet(std::initializer_list<TokenKind> kinds) noexcept
      : words_{} {
    for (TokenKind k : kinds) {
      const auto idx = static_cast<std::size_t>(k);
      // Caller is required to pass valid kinds (< tk_count). Out-of-
      // range entries are silently dropped — this is a header-time
      // contract failure that constexpr evaluation would surface.
      if (idx < kBits) {
        words_[idx / 64U] |= (uint64_t{1} << (idx % 64U));
      }
    }
  }

  [[nodiscard]] constexpr bool contains(TokenKind k) const noexcept {
    const auto idx = static_cast<std::size_t>(k);
    if (idx >= kBits) {
      return false;
    }
    return (words_[idx / 64U] & (uint64_t{1} << (idx % 64U))) != 0U;
  }

  /// Set union — used at runtime by `Parser::currentRecoverySet()` to
  /// merge a stack of nested per-rule sets. The result represents
  /// "tokens at which ANY enclosing rule wants to resume": when an
  /// inner rule's recovery hits one of these, control unwinds to the
  /// rule that owns it.
  [[nodiscard]] constexpr TokenSet operator|(TokenSet other) const noexcept {
    TokenSet out;
    for (std::size_t i = 0; i < kWords; ++i) {
      out.words_[i] = words_[i] | other.words_[i];
    }
    return out;
  }

  TokenSet &operator|=(TokenSet other) noexcept {
    for (std::size_t i = 0; i < kWords; ++i) {
      words_[i] |= other.words_[i];
    }
    return *this;
  }

private:
  std::array<uint64_t, kWords> words_;
};

/// Advance the parser's lexer cursor forward (consuming tokens) until
/// `peek().kind() ∈ set` or `peek().kind() == tk_eof`. Returns the
/// kind at which the scan stopped (one of `set`'s members, or
/// `tk_eof`). Deterministic: forward-only scan, no env state.
///
/// The recovery token is NOT consumed — the caller's loop or the
/// rule that owns the set is responsible for deciding whether to
/// consume a `;` (rule-local resync) or leave a `}` / item-keyword
/// in place (block close / next-iteration dispatch).
TokenKind skipUntil(Parser &p, TokenSet set);

/// RAII helper: at construction, pushes `local` onto
/// `Parser::recovery_stack_` (combined with the existing top of stack
/// to form the new "active set"). At destruction, pops the entry.
///
/// Per parser-recovery.contract.md the merged "active set" is the
/// UNION of every `RecoveryGuard` currently on the stack — when an
/// inner rule's recovery hits a token belonging to an OUTER guard,
/// the inner skipUntil() yields, control returns to the inner rule's
/// caller, which sees a parked recovery token and unwinds further.
class RecoveryGuard {
public:
  RecoveryGuard(Parser &p, TokenSet local) noexcept;
  ~RecoveryGuard() noexcept;

  RecoveryGuard(const RecoveryGuard &) = delete;
  RecoveryGuard &operator=(const RecoveryGuard &) = delete;
  RecoveryGuard(RecoveryGuard &&) = delete;
  RecoveryGuard &operator=(RecoveryGuard &&) = delete;

private:
  Parser &p_;
};

// ---------- Recovery sets (per parser-recovery.contract.md
// §"Recovery sets per grammar level"). Each table is `static
// constexpr` so the bitset is materialized at compile time
// (Principle V determinism: byte-identical artifacts).
//
// The contract names tokens as their EBNF spelling (`struct`,
// `declare`, `module`, etc.); the actual TokenKind enumerator names
// use the `tk_<suffix>` form from include/nsl/Lex/KeywordSet.def,
// which adds a trailing `_` for tokens that collide with C++
// keywords (`tk_struct_`, `tk_for_`, `tk_goto_`, `tk_if_`,
// `tk_while_`, `tk_else_`, `tk_return_`). The contract is satisfied
// when the SET matches the listed tokens; the spelling difference
// is purely lexical.
namespace recovery_sets {

/// `parseCompilationUnit()` (top): resume at the next top-level item
/// keyword. Per the contract: `{struct, declare, module, param_int,
/// param_str, Eof}`.
inline constexpr TokenSet kTopLevel{
    TokenKind::tk_struct_,   TokenKind::tk_declare,   TokenKind::tk_module,
    TokenKind::tk_param_int, TokenKind::tk_param_str, TokenKind::tk_eof,
};

/// `parseDeclareItem()` (inside `declare { … }`): per contract
/// `{Semi, RBrace}`. Inner rules see this UNION-ed with the
/// top-level set, so skipUntil also halts at top-level keywords.
inline constexpr TokenSet kDeclareItem{
    TokenKind::tk_semicolon,
    TokenKind::tk_rbrace,
};

/// `parseModuleItem()` (inside `module { … }`): per contract
/// `{Semi, RBrace, func, function, proc, state, wire, reg, mem,
///   integer, variable, proc_name, state_name, first_state,
///   func_self}`.
inline constexpr TokenSet kModuleItem{
    TokenKind::tk_semicolon,  TokenKind::tk_rbrace,
    TokenKind::tk_func,       TokenKind::tk_function,
    TokenKind::tk_proc,       TokenKind::tk_state,
    TokenKind::tk_wire,       TokenKind::tk_reg,
    TokenKind::tk_mem,        TokenKind::tk_integer,
    TokenKind::tk_variable,   TokenKind::tk_proc_name,
    TokenKind::tk_state_name, TokenKind::tk_first_state,
    TokenKind::tk_func_self,
};

/// `parseSeqBlock()` body (inside `seq { … }`): per contract
/// `{Semi, RBrace, if, for, while, goto, return}`.
inline constexpr TokenSet kSeqItem{
    TokenKind::tk_semicolon, TokenKind::tk_rbrace, TokenKind::tk_if_,
    TokenKind::tk_for_,      TokenKind::tk_while_, TokenKind::tk_goto_,
    TokenKind::tk_return_,
};

} // namespace recovery_sets

} // namespace nsl::parse

#endif // NSL_LIB_PARSE_RECOVERY_H
