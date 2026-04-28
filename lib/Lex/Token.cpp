// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// lib/Lex/Token.cpp — implementation of the `toString(TokenKind)`
// helper declared in `include/nsl/Lex/Token.h`. Reserved-keyword cases
// are generated via the `KeywordSet.def` X-macro so adding a keyword
// requires no edit to this file (research §6 single-source-of-truth).

#include "nsl/Lex/Token.h"

#include "llvm/ADT/StringRef.h"

namespace nsl {

llvm::StringRef toString(TokenKind k) {
  switch (k) {
  case TokenKind::tk_unknown:
    return "tk_unknown";
  case TokenKind::tk_eof:
    return "tk_eof";

  case TokenKind::tk_identifier:
    return "tk_identifier";
  case TokenKind::tk_string_lit:
    return "tk_string_lit";

  case TokenKind::tk_decimal_lit:
    return "tk_decimal_lit";
  case TokenKind::tk_hex_lit:
    return "tk_hex_lit";
  case TokenKind::tk_binary_lit:
    return "tk_binary_lit";
  case TokenKind::tk_octal_lit:
    return "tk_octal_lit";

  case TokenKind::tk_system_task:
    return "tk_system_task";
  case TokenKind::tk_system_function:
    return "tk_system_function";
  case TokenKind::tk_unused_underscore:
    return "tk_unused_underscore";

    // Reserved keywords from KeywordSet.def — single source of truth.
#define KEYWORD(name, spelling)                                                \
  case TokenKind::tk_##name:                                                   \
    return "tk_" #name;
#include "nsl/Lex/KeywordSet.def"
#undef KEYWORD

  case TokenKind::tk_lparen:
    return "tk_lparen";
  case TokenKind::tk_rparen:
    return "tk_rparen";
  case TokenKind::tk_lbrace:
    return "tk_lbrace";
  case TokenKind::tk_rbrace:
    return "tk_rbrace";
  case TokenKind::tk_lbracket:
    return "tk_lbracket";
  case TokenKind::tk_rbracket:
    return "tk_rbracket";
  case TokenKind::tk_comma:
    return "tk_comma";
  case TokenKind::tk_semicolon:
    return "tk_semicolon";
  case TokenKind::tk_colon:
    return "tk_colon";
  case TokenKind::tk_dot:
    return "tk_dot";
  case TokenKind::tk_assign:
    return "tk_assign";
  case TokenKind::tk_assign_seq:
    return "tk_assign_seq";
  case TokenKind::tk_plus:
    return "tk_plus";
  case TokenKind::tk_minus:
    return "tk_minus";
  case TokenKind::tk_plus_plus:
    return "tk_plus_plus";
  case TokenKind::tk_minus_minus:
    return "tk_minus_minus";
  case TokenKind::tk_star:
    return "tk_star";
  case TokenKind::tk_slash:
    return "tk_slash";
  case TokenKind::tk_percent:
    return "tk_percent";
  case TokenKind::tk_amp:
    return "tk_amp";
  case TokenKind::tk_pipe:
    return "tk_pipe";
  case TokenKind::tk_caret:
    return "tk_caret";
  case TokenKind::tk_tilde:
    return "tk_tilde";
  case TokenKind::tk_logical_and:
    return "tk_logical_and";
  case TokenKind::tk_logical_or:
    return "tk_logical_or";
  case TokenKind::tk_logical_not:
    return "tk_logical_not";
  case TokenKind::tk_equal:
    return "tk_equal";
  case TokenKind::tk_not_equal:
    return "tk_not_equal";
  case TokenKind::tk_less:
    return "tk_less";
  case TokenKind::tk_less_equal:
    return "tk_less_equal";
  case TokenKind::tk_greater:
    return "tk_greater";
  case TokenKind::tk_greater_equal:
    return "tk_greater_equal";
  case TokenKind::tk_shift_left:
    return "tk_shift_left";
  case TokenKind::tk_shift_right:
    return "tk_shift_right";
  case TokenKind::tk_question:
    return "tk_question";
  case TokenKind::tk_at:
    return "tk_at";
  case TokenKind::tk_hash_sign_extend:
    return "tk_hash_sign_extend";
  case TokenKind::tk_apostrophe_zero_extend:
    return "tk_apostrophe_zero_extend";
  case TokenKind::tk_dot_lbrace:
    return "tk_dot_lbrace";

  case TokenKind::tk_line_directive:
    return "tk_line_directive";

  case TokenKind::tk_count:
    return "tk_count";
  }
  return "tk_unknown";
}

} // namespace nsl
