// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// lib/AST/NodeKindNames.cpp — X-macro-driven enum-to-string table
// for `nsl::ast::NodeKind`. The table is the source of truth for
// the printer's per-node `(NodeKind …)` opening token (Invariant 6
// in `contracts/ast-stability.contract.md`).

#include "nsl/AST/NodeKind.h"

#include "llvm/ADT/StringRef.h"

namespace nsl::ast {

llvm::StringRef toString(NodeKind k) {
  switch (k) {
#define NSL_NODE_KIND(EnumName, BaseClass)                                    \
  case NodeKind::NK_##EnumName:                                               \
    return #EnumName;
#include "nsl/AST/NodeKind.def"
#undef NSL_NODE_KIND
  case NodeKind::NK_count:
    break;
  }
  return "<invalid-NodeKind>";
}

} // namespace nsl::ast
