# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#
# nsl-preprocess — layer 2. Line-oriented preprocessor implementing
# pp.ebnf §§1–4 with helper evaluation (T055), %IDENT% splicing (T057),
# directive parsing (T058), and the include-stack + conditional-stack
# orchestration in `Preprocessor.cpp` (T059). Single public header
# `Preprocessor.h`; the rest are private to lib/Preprocess/.

add_nsl_library(nsl-preprocess
  Preprocessor.cpp
  DirectiveParser.cpp
  MacroTable.cpp
  HelperEvaluator.cpp
  PPExpression.cpp
  IdentSplicer.cpp
  HEADERS
    ${CMAKE_SOURCE_DIR}/include/nsl/Preprocess/Preprocessor.h
    ${CMAKE_SOURCE_DIR}/include/nsl/Preprocess/MacroTable.h
    ${CMAKE_SOURCE_DIR}/include/nsl/Preprocess/HelperEvaluator.h
  DEPENDS
    nsl-basic
  LINK_LIBS
    LLVMSupport)
