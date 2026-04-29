// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// include/nsl/AST/DeclareBlock.h — `declare_block`
// (`lang.ebnf §4`; data-model §1.3). Fields: optional `name`,
// optional `modifier` (`Interface`/`Simulation`/`None` — S20
// interface modifier), `headerParams` (parameter declarations), and
// `ports` (data + control terminals).

#ifndef NSL_AST_DECLARE_BLOCK_H
#define NSL_AST_DECLARE_BLOCK_H

#include "nsl/AST/ASTNode.h"
#include "nsl/AST/Decl.h"
#include "nsl/AST/PortDecl.h"

#include <memory>
#include <utility>
#include <vector>

namespace nsl::ast {

class DeclareBlock final : public Decl {
public:
  enum class Modifier { None, Interface, Simulation };

  DeclareBlock(SourceRange range, Identifier name, Modifier modifier,
               std::vector<std::unique_ptr<Decl>> headerParams,
               std::vector<std::unique_ptr<PortDecl>> ports)
      : Decl(NodeKind::NK_DeclareBlock, range), name_(name),
        modifier_(modifier), headerParams_(std::move(headerParams)),
        ports_(std::move(ports)) {}

  DeclareBlock(SourceRange range, Identifier name, Modifier modifier,
               Identifier clockName, Identifier resetName,
               std::vector<std::unique_ptr<Decl>> headerParams,
               std::vector<std::unique_ptr<PortDecl>> ports)
      : Decl(NodeKind::NK_DeclareBlock, range), name_(name),
        modifier_(modifier), clockName_(clockName), resetName_(resetName),
        headerParams_(std::move(headerParams)), ports_(std::move(ports)) {}

  /// Empty `Identifier` (a default-constructed `StringRef`) means
  /// the `declare` block was anonymous.
  [[nodiscard]] Identifier name() const noexcept { return name_; }
  [[nodiscard]] Modifier modifier() const noexcept { return modifier_; }
  /// `interface(clock=<name>, reset=<name>)` modifier args; empty
  /// `Identifier` if absent. Used by S20 to verify both names are
  /// present whenever the modifier is `Interface`.
  [[nodiscard]] Identifier clockName() const noexcept { return clockName_; }
  [[nodiscard]] Identifier resetName() const noexcept { return resetName_; }
  [[nodiscard]] const std::vector<std::unique_ptr<Decl>> &
  headerParams() const noexcept {
    return headerParams_;
  }
  [[nodiscard]] const std::vector<std::unique_ptr<PortDecl>> &
  ports() const noexcept {
    return ports_;
  }

  NSL_AST_NODE_BOILERPLATE(DeclareBlock)

private:
  Identifier name_;
  Modifier modifier_;
  Identifier clockName_;
  Identifier resetName_;
  std::vector<std::unique_ptr<Decl>> headerParams_;
  std::vector<std::unique_ptr<PortDecl>> ports_;
};

} // namespace nsl::ast

#endif // NSL_AST_DECLARE_BLOCK_H
