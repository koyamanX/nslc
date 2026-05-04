// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// include/nsl/AST/TopLevelParamDecl.h — `top_level_parameter`
// (`lang.ebnf §3.1`; data-model §1.2). Fields: `kind`
// (`Int` or `Str` — `param_int` vs `param_str`), `name`, `init`
// (constant-expression initializer; string-literal for `Str`).

#ifndef NSL_AST_TOP_LEVEL_PARAM_DECL_H
#define NSL_AST_TOP_LEVEL_PARAM_DECL_H

#include "nsl/AST/ASTNode.h"
#include "nsl/AST/Decl.h"
#include "nsl/AST/Expr.h"

#include <memory>
#include <utility>

namespace nsl::ast {

class TopLevelParamDecl final : public Decl {
public:
  enum class ParamKind { Int, Str };

  TopLevelParamDecl(SourceRange range, ParamKind k, Identifier name,
                    std::unique_ptr<Expr> init)
      : Decl(NodeKind::NK_TopLevelParamDecl, range), paramKind_(k), name_(name),
        init_(std::move(init)) {}

  [[nodiscard]] ParamKind paramKind() const noexcept { return paramKind_; }
  [[nodiscard]] Identifier name() const noexcept { return name_; }
  [[nodiscard]] const Expr *init() const noexcept { return init_.get(); }

  NSL_AST_NODE_BOILERPLATE(TopLevelParamDecl)

private:
  ParamKind paramKind_;
  Identifier name_;
  std::unique_ptr<Expr> init_;
};

} // namespace nsl::ast

#endif // NSL_AST_TOP_LEVEL_PARAM_DECL_H
