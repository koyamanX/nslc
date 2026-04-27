# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#
# nsl-lex — layer 3. Lexical analysis (identifiers, numbers Z/X/U,
# strings, _-prefix names, keywords, N5 `#` disambiguation,
# `tk_line_directive` pass-through). Real lexer at M1 (T026–T031).

add_nsl_library(nsl-lex
  Lexer.cpp
  Token.cpp
  KeywordSet.cpp
  NumberLiteral.cpp
  HEADERS
    ${CMAKE_SOURCE_DIR}/include/nsl/Lex/Token.h
    ${CMAKE_SOURCE_DIR}/include/nsl/Lex/Lexer.h
    ${CMAKE_SOURCE_DIR}/include/nsl/Lex/KeywordSet.h
    ${CMAKE_SOURCE_DIR}/include/nsl/Lex/KeywordSet.def
  DEPENDS
    nsl-basic
  LINK_LIBS
    LLVMSupport)
