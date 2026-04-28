// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// include/nsl/AST/SubmoduleDecl.h — `submodule_declaration`
// (`lang.ebnf §6`; data-model §1.4). Fields: `templateName` (the
// referenced declare-block name), `instances` (one entry per
// declared instance — name + optional array size), `paramAssigns`
// (parenthesized parameter-assignment list, optional).

#ifndef NSL_AST_SUBMODULE_DECL_H
#define NSL_AST_SUBMODULE_DECL_H

#include "nsl/AST/ASTNode.h"
#include "nsl/AST/Decl.h"
#include "nsl/AST/Expr.h"

#include <memory>
#include <utility>
#include <vector>

namespace nsl::ast {

class SubmoduleDecl final : public Decl {
public:
  /// One submodule instance: a name and an optional array-size
  /// expression (the `[N]` form). `arraySize` is nullptr for
  /// non-array instances.
  struct Instance {
    Identifier name;
    std::unique_ptr<Expr> arraySize;
  };

  /// One parameter assignment: `name = value` (Verilog-flavored).
  struct ParamAssign {
    Identifier name;
    std::unique_ptr<Expr> value;
  };

  SubmoduleDecl(SourceRange range, Identifier templateName,
                std::vector<Instance> instances,
                std::vector<ParamAssign> paramAssigns)
      : Decl(NodeKind::NK_SubmoduleDecl, range),
        templateName_(templateName), instances_(std::move(instances)),
        paramAssigns_(std::move(paramAssigns)) {}

  [[nodiscard]] Identifier templateName() const noexcept {
    return templateName_;
  }
  [[nodiscard]] const std::vector<Instance> &instances() const noexcept {
    return instances_;
  }
  [[nodiscard]] const std::vector<ParamAssign> &
  paramAssigns() const noexcept {
    return paramAssigns_;
  }

  NSL_AST_NODE_BOILERPLATE(SubmoduleDecl)

private:
  Identifier templateName_;
  std::vector<Instance> instances_;
  std::vector<ParamAssign> paramAssigns_;
};

} // namespace nsl::ast

#endif // NSL_AST_SUBMODULE_DECL_H
