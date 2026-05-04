// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// include/nsl/AST/PortDecl.h — port declaration appearing inside a
// `declare` block (`lang.ebnf §4`; data-model §1.3). Models both
// data terminals (`input`/`output`/`inout` with optional width) AND
// control terminals (`func_in`/`func_out` with optional dummy-arg
// list and optional return-terminal). Per data-model §1.3 a
// separate `ControlTerminalDecl` AST node was considered but is NOT
// in `NodeKind.def` at M2 — control vs data is one extra
// `Direction` enumerator instead.

#ifndef NSL_AST_PORT_DECL_H
#define NSL_AST_PORT_DECL_H

#include "nsl/AST/ASTNode.h"
#include "nsl/AST/Decl.h"
#include "nsl/AST/Expr.h"

#include <memory>
#include <utility>
#include <vector>

namespace nsl::ast {

class PortDecl final : public Decl {
public:
  /// Seven directions: three data (Input/Output/Inout), two
  /// control (FuncIn/FuncOut), one wire-class internal terminal
  /// (Wire) referenced by func_self dummy args, and one control
  /// (FuncSelf). Discriminator for the parser-built shape. The
  /// audited NSL fixture corpus accepts `wire` and `func_self`
  /// inside declare blocks even though the strict EBNF data/
  /// control_direction enums in `lang.ebnf §4` don't list them
  /// — Sema's S4 checker enforces the dummy-arg-direction rule
  /// across all kinds.
  enum class Direction {
    Input,
    Output,
    Inout,
    FuncIn,
    FuncOut,
    Wire,
    FuncSelf,
  };

  PortDecl(SourceRange range, Direction direction, Identifier name,
           std::unique_ptr<Expr> width, std::vector<Identifier> dummyArgs,
           Identifier returnTerminal)
      : Decl(NodeKind::NK_PortDecl, range), direction_(direction), name_(name),
        width_(std::move(width)), dummyArgs_(std::move(dummyArgs)),
        returnTerminal_(returnTerminal) {}

  [[nodiscard]] Direction direction() const noexcept { return direction_; }
  [[nodiscard]] Identifier name() const noexcept { return name_; }
  [[nodiscard]] const Expr *width() const noexcept { return width_.get(); }
  /// Empty for data terminals.
  [[nodiscard]] const std::vector<Identifier> &dummyArgs() const noexcept {
    return dummyArgs_;
  }
  /// Empty `StringRef` for data terminals or control terminals
  /// without a return-terminal annotation.
  [[nodiscard]] Identifier returnTerminal() const noexcept {
    return returnTerminal_;
  }

  NSL_AST_NODE_BOILERPLATE(PortDecl)

private:
  Direction direction_;
  Identifier name_;
  std::unique_ptr<Expr> width_;
  std::vector<Identifier> dummyArgs_;
  Identifier returnTerminal_;
};

} // namespace nsl::ast

#endif // NSL_AST_PORT_DECL_H
