// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// include/nsl/AST/ModuleBlock.h — `module_block`
// (`lang.ebnf §5`; data-model §1.4). Fields: `name` and four
// declaration-order vectors holding the body items dispatched per
// kind: `internals` (reg/wire/mem/...), `actions` (top-level
// `par`/`alt`/`seq`/...), `funcs` (`FuncDefn`s), `procs`
// (`ProcDefn` + `StateDefn`s). The four-vector split mirrors
// `nsl_compiler_design.md §5` lines 339–344.

#ifndef NSL_AST_MODULE_BLOCK_H
#define NSL_AST_MODULE_BLOCK_H

#include "nsl/AST/ASTNode.h"
#include "nsl/AST/Decl.h"
#include "nsl/AST/Stmt.h"

#include <memory>
#include <utility>
#include <vector>

namespace nsl::ast {

class ModuleBlock final : public Decl {
public:
  ModuleBlock(SourceRange range, Identifier name,
              std::vector<std::unique_ptr<Decl>> internals,
              std::vector<std::unique_ptr<Stmt>> actions,
              std::vector<std::unique_ptr<Decl>> funcs,
              std::vector<std::unique_ptr<Decl>> procs)
      : Decl(NodeKind::NK_ModuleBlock, range), name_(name),
        internals_(std::move(internals)), actions_(std::move(actions)),
        funcs_(std::move(funcs)), procs_(std::move(procs)) {}

  [[nodiscard]] Identifier name() const noexcept { return name_; }
  [[nodiscard]] const std::vector<std::unique_ptr<Decl>> &
  internals() const noexcept {
    return internals_;
  }
  [[nodiscard]] const std::vector<std::unique_ptr<Stmt>> &
  actions() const noexcept {
    return actions_;
  }
  [[nodiscard]] const std::vector<std::unique_ptr<Decl>> &
  funcs() const noexcept {
    return funcs_;
  }
  [[nodiscard]] const std::vector<std::unique_ptr<Decl>> &
  procs() const noexcept {
    return procs_;
  }

  NSL_AST_NODE_BOILERPLATE(ModuleBlock)

private:
  Identifier name_;
  std::vector<std::unique_ptr<Decl>> internals_;
  std::vector<std::unique_ptr<Stmt>> actions_;
  std::vector<std::unique_ptr<Decl>> funcs_;
  std::vector<std::unique_ptr<Decl>> procs_;
};

} // namespace nsl::ast

#endif // NSL_AST_MODULE_BLOCK_H
