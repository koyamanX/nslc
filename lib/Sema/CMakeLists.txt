# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#
# nsl-sema — layer 6. Semantic analysis (S1..S29 constraints from
# docs/spec/nsl_lang.ebnf). M3 Phase 2 ships the scaffolding:
# SymbolTable + TypeSystem + Sema engine entry points. Phase 3 (US1)
# adds `ResolutionPass.cpp` (resolution + width inference); Phase 4
# (US2) adds the 29 `Constraints/S<NN>_*.cpp` walkers.
#
# Per the amended Constitution Principle II §3 (v1.6.0), `nsl-sema`
# is a NAMED exception to the single-public-header rule alongside
# `nsl-ast`. Three umbrella public headers are permitted: `Sema.h`,
# `SymbolTable.h`, `TypeSystem.h`. Symbol subclasses nest INSIDE
# `SymbolTable.h` (NOT as separate per-kind headers); type subclasses
# nest INSIDE `TypeSystem.h`. The X-macro source-of-truth file
# `SymbolKind.def` parallels `nsl-ast`'s `NodeKind.def`.

add_nsl_library(nsl-sema
  Sema.cpp
  SymbolTable.cpp
  TypeSystem.cpp
  ResolutionPass.cpp
  ConstraintCheckRegistry.cpp
  HEADERS
    ${CMAKE_SOURCE_DIR}/include/nsl/Sema/Sema.h
    ${CMAKE_SOURCE_DIR}/include/nsl/Sema/SymbolTable.h
    ${CMAKE_SOURCE_DIR}/include/nsl/Sema/TypeSystem.h
    ${CMAKE_SOURCE_DIR}/include/nsl/Sema/SymbolKind.def
  DEPENDS
    nsl-basic
    nsl-ast)
