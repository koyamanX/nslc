# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#
# nsl-basic — layer 1. Foundation: SourceLocation/SourceRange/FileID
# (entities 1–3 of specs/002-m1-lex-preprocess/data-model.md),
# SourceManager (entities 4 + 5), DiagnosticEngine (entity 6), and
# the X-macro single-source-of-truth `HelperSet.def` consumed by
# downstream layers.

add_nsl_library(nsl-basic
  SourceLocation.cpp
  SourceManager.cpp
  Diagnostic.cpp
  HEADERS
    ${CMAKE_SOURCE_DIR}/include/nsl/Basic/SourceLocation.h
    ${CMAKE_SOURCE_DIR}/include/nsl/Basic/SourceManager.h
    ${CMAKE_SOURCE_DIR}/include/nsl/Basic/Diagnostic.h
    ${CMAKE_SOURCE_DIR}/include/nsl/Basic/HelperSet.def
  LINK_LIBS
    LLVMSupport)
