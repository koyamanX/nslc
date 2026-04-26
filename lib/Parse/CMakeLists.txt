# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#
# nsl-parse — layer 5. Recursive-descent parser building nsl-ast
# nodes from the nsl-lex token stream. M0 skeleton; real parser at M2.

add_nsl_library(nsl-parse
  DEPENDS nsl-lex nsl-ast)
