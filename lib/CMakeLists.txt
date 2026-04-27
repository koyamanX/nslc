# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#
# lib/CMakeLists.txt — aggregator for the nine compiler-track layer
# libraries declared via `add_nsl_library` (cmake/AddNSLLibrary.cmake).
#
# Order matches `docs/design/nsl_compiler_design.md` §3 with one
# adjustment: nsl-ast precedes nsl-parse because nsl-parse declares
# `DEPENDS nsl-ast` per data-model.md §entity 1.
#
# Adding a tenth library is one new add_subdirectory() line plus one
# `add_nsl_library(...)` invocation in the new dir — no top-level
# `CMakeLists.txt` edits required (Constitution Principle II / SC-008).

add_subdirectory(Basic)
add_subdirectory(Preprocess)
add_subdirectory(Lex)
add_subdirectory(AST)
add_subdirectory(Parse)
add_subdirectory(Sema)
add_subdirectory(Dialect/NSL/IR)
add_subdirectory(Lower)
add_subdirectory(Driver)
