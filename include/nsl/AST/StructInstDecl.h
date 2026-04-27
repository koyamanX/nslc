// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// include/nsl/AST/StructInstDecl.h — `struct_instance_declaration`
// (`lang.ebnf §6`; data-model §1.4). Fields: `typeName` (the
// referenced struct type), `kind` (Reg or Wire — the
// instance lifetime), optional `arraySize` (the `[N]` form),
// optional `init` (parenthesized initial-value list, one Expr per
// member).

#ifndef NSL_AST_STRUCT_INST_DECL_H
#define NSL_AST_STRUCT_INST_DECL_H

#include "nsl/AST/ASTNode.h"
#include "nsl/AST/Decl.h"
#include "nsl/AST/Expr.h"

#include <memory>
#include <utility>
#include <vector>

namespace nsl::ast {

class StructInstDecl final : public Decl {
public:
  enum class StorageKind { Reg, Wire };

  StructInstDecl(SourceRange range, Identifier typeName,
                 Identifier instanceName, StorageKind kind,
                 std::unique_ptr<Expr> arraySize,
                 std::vector<std::unique_ptr<Expr>> init)
      : Decl(NodeKind::NK_StructInstDecl, range), typeName_(typeName),
        instanceName_(instanceName), storageKind_(kind),
        arraySize_(std::move(arraySize)), init_(std::move(init)) {}

  [[nodiscard]] Identifier typeName() const noexcept { return typeName_; }
  [[nodiscard]] Identifier instanceName() const noexcept {
    return instanceName_;
  }
  [[nodiscard]] StorageKind storageKind() const noexcept {
    return storageKind_;
  }
  [[nodiscard]] const Expr *arraySize() const noexcept {
    return arraySize_.get();
  }
  [[nodiscard]] const std::vector<std::unique_ptr<Expr>> &
  init() const noexcept {
    return init_;
  }

  NSL_AST_NODE_BOILERPLATE(StructInstDecl)

private:
  Identifier typeName_;
  Identifier instanceName_;
  StorageKind storageKind_;
  std::unique_ptr<Expr> arraySize_;
  std::vector<std::unique_ptr<Expr>> init_;
};

} // namespace nsl::ast

#endif // NSL_AST_STRUCT_INST_DECL_H
