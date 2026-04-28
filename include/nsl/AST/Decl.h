// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// include/nsl/AST/Decl.h
//
// `Decl` — abstract mid-level base for "name + body" declarations
// (data-model §1.1, §§1.2–1.4). Concrete subclasses live in their
// own per-kind headers (Principle II §3 exception): `StructDecl`,
// `RegDecl`, `WireDecl`, `ModuleBlock`, `FuncDefn`, etc.
//
// `Decl` itself is empty beyond the `ASTNode` interface — the spec
// (data-model §1.1) lists `Identifier name` "(most kinds)" but a
// few `Decl` subclasses (e.g., `DeclareBlock` whose name is
// optional, `StateNameDecl` whose payload is a vector of names)
// don't fit that mold cleanly. Each per-kind header carries its
// own `Identifier name_` (or equivalent) field.

#ifndef NSL_AST_DECL_H
#define NSL_AST_DECL_H

#include "nsl/AST/ASTNode.h"

namespace nsl::ast {

/// Abstract mid-level base for declaration AST nodes.
class Decl : public ASTNode {
protected:
  using ASTNode::ASTNode;
};

} // namespace nsl::ast

#endif // NSL_AST_DECL_H
