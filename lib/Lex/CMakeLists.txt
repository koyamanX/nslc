# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#
# nsl-lex — layer 3. Lexical analysis (identifiers, numbers Z/X/U,
# strings, _-prefix names, keywords). M0 skeleton; real lexer at M1.

add_nsl_library(nsl-lex
  DEPENDS nsl-basic)
