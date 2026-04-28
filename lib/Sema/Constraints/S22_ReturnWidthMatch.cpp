// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// lib/Sema/Constraints/S22_ReturnWidthMatch.cpp - S22 checker.
// Spec: lang.ebnf:931 — `return` rules:
//   - may appear only inside a `func` body;
//   - bare `return;` valid only when func has no return-value terminal;
//   - return-expression width must match func return-value-terminal.

#include "../ConstraintCheckRegistry.h"
#include "ConstraintHelpers.h"

#include "nsl/AST/CompilationUnit.h"
#include "nsl/AST/DeclareBlock.h"
#include "nsl/AST/FuncDefn.h"
#include "nsl/AST/ModuleBlock.h"
#include "nsl/AST/PortDecl.h"
#include "nsl/AST/ReturnStmt.h"
#include "nsl/Basic/Diagnostic.h"
#include "nsl/Sema/SymbolTable.h"
#include "nsl/Sema/TypeSystem.h"

#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/StringRef.h"

#include <cstdint>
#include <string>

namespace nsl::sema {
namespace {

uint64_t exprWidth(const ast::Expr *e) noexcept {
  if (e == nullptr || e->inferredType() == nullptr) {
    return 0U;
  }
  const Type *t = e->inferredType();
  if (t->kind() == TypeKind::BitVector) {
    return static_cast<const BitVectorType *>(t)->width();
  }
  if (t->kind() == TypeKind::Bit) {
    return 1U;
  }
  return 0U;
}

class S22Visitor : public ConstraintVisitor {
public:
  void run(const ConstraintContext &ctx) const override {
    if (ctx.unit == nullptr || ctx.diag == nullptr) {
      return;
    }
    // Build map of func-name -> return-terminal width (0 if none).
    // Use the declare block's port declarations to resolve return
    // terminals.
    llvm::DenseMap<llvm::StringRef, uint64_t> portWidths;
    llvm::DenseMap<llvm::StringRef, llvm::StringRef> funcReturn;
    for (const auto &item : ctx.unit->items()) {
      if (!item ||
          item->kind() != ast::NodeKind::NK_DeclareBlock) {
        continue;
      }
      const auto &db = static_cast<const ast::DeclareBlock &>(*item);
      for (const auto &p : db.ports()) {
        if (!p || p->kind() != ast::NodeKind::NK_PortDecl) {
          continue;
        }
        const auto &pd = static_cast<const ast::PortDecl &>(*p);
        // Look up width from the symbol table (Type was set by
        // ResolutionPass).
        Symbol *psym = ctx.symbols->lookup(pd.name());
        uint64_t w = 1U;
        if (psym != nullptr && psym->type() != nullptr) {
          const Type *t = psym->type();
          if (t->kind() == TypeKind::BitVector) {
            w = static_cast<const BitVectorType *>(t)->width();
          }
        }
        portWidths[pd.name()] = w;
        if (pd.direction() == ast::PortDecl::Direction::FuncIn ||
            pd.direction() == ast::PortDecl::Direction::FuncOut) {
          if (!pd.returnTerminal().empty()) {
            funcReturn[pd.name()] = pd.returnTerminal();
          }
        }
      }
    }

    detail::walkUnit(
        *ctx.unit, /*dcb=*/nullptr,
        [&](const ast::Stmt &s, uint32_t lex) {
          if (s.kind() != ast::NodeKind::NK_ReturnStmt) {
            return;
          }
          const auto &r = static_cast<const ast::ReturnStmt &>(s);
          if (!detail::has(lex, detail::LexCtx::InFunc)) {
            ctx.diag->report(
                Severity::Error, r.loc().begin(),
                "'return' may appear only inside a 'func' body (S22)");
            return;
          }
          // Width-mismatch / bare-return-with-terminal: at M3 we
          // can't easily plumb the enclosing FuncDefn's name into the
          // walker (the lex-context doesn't carry the func name). We
          // approximate by scanning every FuncDefn whose body
          // textually encloses this Return — the cheap check is to
          // re-walk per-FuncDefn instead.
          (void)r;
          (void)funcReturn;
          (void)portWidths;
        });

    // Per-FuncDefn walk: for each func, find all returns and check
    // them against the func's return-terminal.
    for (const auto &item : ctx.unit->items()) {
      if (!item ||
          item->kind() != ast::NodeKind::NK_ModuleBlock) {
        continue;
      }
      const auto &mb = static_cast<const ast::ModuleBlock &>(*item);
      for (const auto &f : mb.funcs()) {
        if (!f || f->kind() != ast::NodeKind::NK_FuncDefn) {
          continue;
        }
        const auto &fd = static_cast<const ast::FuncDefn &>(*f);
        if (fd.name().parts.empty() || fd.body() == nullptr) {
          continue;
        }
        llvm::StringRef fname = fd.name().parts.back();
        uint64_t terminal_w = 0U;
        bool has_terminal = false;
        auto it = funcReturn.find(fname);
        if (it != funcReturn.end()) {
          has_terminal = true;
          auto pwit = portWidths.find(it->second);
          if (pwit != portWidths.end()) {
            terminal_w = pwit->second;
          }
        }
        detail::walkStmt(
            *fd.body(), 0U,
            [&](const ast::Stmt &s, uint32_t /*lex*/) {
              if (s.kind() != ast::NodeKind::NK_ReturnStmt) {
                return;
              }
              const auto &r = static_cast<const ast::ReturnStmt &>(s);
              if (r.value() == nullptr) {
                if (has_terminal) {
                  ctx.diag->report(
                      Severity::Error, r.loc().begin(),
                      "bare 'return;' is valid only when the func "
                      "has no return-value terminal (S22)");
                }
                return;
              }
              if (has_terminal) {
                uint64_t rw = exprWidth(r.value());
                if (rw != 0U && terminal_w != 0U && rw != terminal_w) {
                  std::string msg =
                      "return-expression width " + std::to_string(rw) +
                      " does not match func's return-value-terminal "
                      "width " +
                      std::to_string(terminal_w) + " (S22)";
                  ctx.diag->report(Severity::Error, r.loc().begin(),
                                   std::move(msg));
                }
              }
            });
      }
    }
  }
};

} // namespace
} // namespace nsl::sema

NSL_REGISTER_CONSTRAINT(22, S22Visitor)
