# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#
# nsl-ast — layer 4. AST node hierarchy with the per-node-kind
# headers exception (Principle II). M0 skeleton; nodes arrive
# incrementally with M2 (parser) and M3 (sema).

add_nsl_library(nsl-ast
  DEPENDS nsl-basic)
