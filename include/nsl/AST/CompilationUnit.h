// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// include/nsl/AST/CompilationUnit.h — root of an NSL compilation
// unit (data-model §1.2). Field: `items` — vector of top-level
// declarations (`StructDecl`, `DeclareBlock`, `ModuleBlock`,
// `TopLevelParamDecl`) in declaration order. `line_marker`
// directives are consumed by the parser (no AST node) per FR-015 /
// N14 — they don't appear in `items`.

#ifndef NSL_AST_COMPILATION_UNIT_H
#define NSL_AST_COMPILATION_UNIT_H

#include "nsl/AST/ASTNode.h"
#include "nsl/AST/Decl.h"

#include <memory>
#include <utility>
#include <vector>

namespace nsl::ast {

/// AST root: zero or more top-level items in declaration order.
class CompilationUnit final : public ASTNode {
public:
  CompilationUnit(SourceRange range,
                  std::vector<std::unique_ptr<Decl>> items)
      : ASTNode(NodeKind::NK_CompilationUnit, range),
        items_(std::move(items)) {}

  [[nodiscard]] const std::vector<std::unique_ptr<Decl>> &items() const noexcept {
    return items_;
  }

  NSL_AST_NODE_BOILERPLATE(CompilationUnit)

private:
  std::vector<std::unique_ptr<Decl>> items_;
};

} // namespace nsl::ast

#endif // NSL_AST_COMPILATION_UNIT_H
