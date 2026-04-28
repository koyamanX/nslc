// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// lib/Sema/Constraints/ConstraintHelpers.h - shared private helpers
// for the per-Sn constraint TUs. NOT a public header; lives next
// to the per-Sn sources.
//
// Provides a generic "walk every Stmt under a Decl" callback so
// each Sn TU can register a small visitor closure without
// reimplementing the lexical-context bookkeeping.

#ifndef NSL_SEMA_CONSTRAINTS_CONSTRAINT_HELPERS_H
#define NSL_SEMA_CONSTRAINTS_CONSTRAINT_HELPERS_H

#include "nsl/AST/AltBlock.h"
#include "nsl/AST/AnyBlock.h"
#include "nsl/AST/CompilationUnit.h"
#include "nsl/AST/ControlCallStmt.h"
#include "nsl/AST/DeclareBlock.h"
#include "nsl/AST/Expr.h"
#include "nsl/AST/FirstStateDecl.h"
#include "nsl/AST/ForBlock.h"
#include "nsl/AST/FuncDefn.h"
#include "nsl/AST/FuncSelfDecl.h"
#include "nsl/AST/IfStmt.h"
#include "nsl/AST/InitBlockStmt.h"
#include "nsl/AST/IntegerDecl.h"
#include "nsl/AST/LabeledStmt.h"
#include "nsl/AST/MemDecl.h"
#include "nsl/AST/ModuleBlock.h"
#include "nsl/AST/NodeKind.h"
#include "nsl/AST/ParallelBlock.h"
#include "nsl/AST/PortDecl.h"
#include "nsl/AST/ProcDefn.h"
#include "nsl/AST/ProcNameDecl.h"
#include "nsl/AST/RegDecl.h"
#include "nsl/AST/ReturnStmt.h"
#include "nsl/AST/SeqBlock.h"
#include "nsl/AST/StateDefn.h"
#include "nsl/AST/StateNameDecl.h"
#include "nsl/AST/Stmt.h"
#include "nsl/AST/StructDecl.h"
#include "nsl/AST/StructInstDecl.h"
#include "nsl/AST/StructuralGenerate.h"
#include "nsl/AST/SubmoduleDecl.h"
#include "nsl/AST/SystemTaskStmt.h"
#include "nsl/AST/TopLevelParamDecl.h"
#include "nsl/AST/TransferStmt.h"
#include "nsl/AST/VariableDecl.h"
#include "nsl/AST/WhileBlock.h"
#include "nsl/AST/WireDecl.h"

#include <cstdint>
#include <functional>

namespace nsl::sema::detail {

/// Lexical context bits tracked while walking a Stmt subtree.
enum class LexCtx : uint32_t {
  None         = 0U,
  InModule     = 1U << 0U,
  InFunc       = 1U << 1U,
  InProc       = 1U << 2U,
  InState      = 1U << 3U,
  InSeq        = 1U << 4U,
  InParallel   = 1U << 5U,
  InAlt        = 1U << 6U,
  InAny        = 1U << 7U,
  InIf         = 1U << 8U,
  InWhile      = 1U << 9U,
  InFor        = 1U << 10U,
  InInitBlock  = 1U << 11U,
  InGenerate   = 1U << 12U,
  /// True when we have walked under ANY action block — i.e., the
  /// position is no longer "directly inside the module body".
  InAnyAction  = 1U << 13U,
};

inline uint32_t toRaw(LexCtx c) noexcept { return static_cast<uint32_t>(c); }
inline uint32_t opOr(uint32_t a, LexCtx b) noexcept {
  return a | toRaw(b);
}
inline bool has(uint32_t bits, LexCtx b) noexcept {
  return (bits & toRaw(b)) != 0U;
}

using StmtCb = std::function<void(const ast::Stmt &, uint32_t)>;
using DeclCb = std::function<void(const ast::Decl &, uint32_t)>;

inline void walkStmt(const ast::Stmt &s, uint32_t ctx, const StmtCb &cb) {
  cb(s, ctx);
  switch (s.kind()) {
  case ast::NodeKind::NK_SeqBlock: {
    const auto &n = static_cast<const ast::SeqBlock &>(s);
    uint32_t c = opOr(ctx | toRaw(LexCtx::InAnyAction), LexCtx::InSeq);
    for (const auto &it : n.items()) {
      if (it) {
        walkStmt(*it, c, cb);
      }
    }
    break;
  }
  case ast::NodeKind::NK_ParallelBlock: {
    const auto &n = static_cast<const ast::ParallelBlock &>(s);
    uint32_t c = opOr(ctx | toRaw(LexCtx::InAnyAction), LexCtx::InParallel);
    for (const auto &it : n.items()) {
      if (it) {
        walkStmt(*it, c, cb);
      }
    }
    break;
  }
  case ast::NodeKind::NK_AltBlock: {
    const auto &n = static_cast<const ast::AltBlock &>(s);
    uint32_t c = opOr(ctx | toRaw(LexCtx::InAnyAction), LexCtx::InAlt);
    for (const auto &cc : n.cases()) {
      if (cc.body) {
        walkStmt(*cc.body, c, cb);
      }
    }
    if (n.elseCase()) {
      walkStmt(*n.elseCase(), c, cb);
    }
    break;
  }
  case ast::NodeKind::NK_AnyBlock: {
    const auto &n = static_cast<const ast::AnyBlock &>(s);
    uint32_t c = opOr(ctx | toRaw(LexCtx::InAnyAction), LexCtx::InAny);
    for (const auto &cc : n.cases()) {
      if (cc.body) {
        walkStmt(*cc.body, c, cb);
      }
    }
    if (n.elseCase()) {
      walkStmt(*n.elseCase(), c, cb);
    }
    break;
  }
  case ast::NodeKind::NK_IfStmt: {
    const auto &n = static_cast<const ast::IfStmt &>(s);
    uint32_t c = opOr(ctx | toRaw(LexCtx::InAnyAction), LexCtx::InIf);
    if (n.thenBr()) {
      walkStmt(*n.thenBr(), c, cb);
    }
    if (n.elseBr()) {
      walkStmt(*n.elseBr(), c, cb);
    }
    break;
  }
  case ast::NodeKind::NK_WhileBlock: {
    const auto &n = static_cast<const ast::WhileBlock &>(s);
    uint32_t c = opOr(ctx | toRaw(LexCtx::InAnyAction), LexCtx::InWhile);
    for (const auto &it : n.items()) {
      if (it) {
        walkStmt(*it, c, cb);
      }
    }
    break;
  }
  case ast::NodeKind::NK_ForBlock: {
    const auto &n = static_cast<const ast::ForBlock &>(s);
    uint32_t c = opOr(ctx | toRaw(LexCtx::InAnyAction), LexCtx::InFor);
    for (const auto &it : n.items()) {
      if (it) {
        walkStmt(*it, c, cb);
      }
    }
    break;
  }
  case ast::NodeKind::NK_InitBlockStmt: {
    const auto &n = static_cast<const ast::InitBlockStmt &>(s);
    uint32_t c =
        opOr(ctx | toRaw(LexCtx::InAnyAction), LexCtx::InInitBlock);
    for (const auto &it : n.items()) {
      if (it) {
        walkStmt(*it, c, cb);
      }
    }
    break;
  }
  case ast::NodeKind::NK_LabeledStmt: {
    const auto &n = static_cast<const ast::LabeledStmt &>(s);
    if (n.body()) {
      walkStmt(*n.body(), ctx | toRaw(LexCtx::InAnyAction), cb);
    }
    break;
  }
  case ast::NodeKind::NK_StructuralGenerate: {
    const auto &n = static_cast<const ast::StructuralGenerate &>(s);
    uint32_t c = opOr(ctx | toRaw(LexCtx::InAnyAction), LexCtx::InGenerate);
    if (n.body()) {
      walkStmt(*n.body(), c, cb);
    }
    break;
  }
  default:
    break;
  }
}

inline void walkDecl(const ast::Decl &d, uint32_t ctx, const DeclCb &dcb,
                     const StmtCb &scb) {
  if (dcb) {
    dcb(d, ctx);
  }
  switch (d.kind()) {
  case ast::NodeKind::NK_DeclareBlock: {
    const auto &n = static_cast<const ast::DeclareBlock &>(d);
    for (const auto &p : n.headerParams()) {
      if (p) {
        walkDecl(*p, ctx, dcb, scb);
      }
    }
    for (const auto &p : n.ports()) {
      if (p) {
        walkDecl(*p, ctx, dcb, scb);
      }
    }
    break;
  }
  case ast::NodeKind::NK_ModuleBlock: {
    const auto &n = static_cast<const ast::ModuleBlock &>(d);
    uint32_t c = ctx | toRaw(LexCtx::InModule);
    for (const auto &i : n.internals()) {
      if (i) {
        walkDecl(*i, c, dcb, scb);
      }
    }
    for (const auto &f : n.funcs()) {
      if (f) {
        walkDecl(*f, c, dcb, scb);
      }
    }
    for (const auto &p : n.procs()) {
      if (p) {
        walkDecl(*p, c, dcb, scb);
      }
    }
    if (scb) {
      for (const auto &a : n.actions()) {
        if (a) {
          walkStmt(*a, c, scb);
        }
      }
    }
    break;
  }
  case ast::NodeKind::NK_FuncDefn: {
    const auto &n = static_cast<const ast::FuncDefn &>(d);
    uint32_t c = ctx | toRaw(LexCtx::InFunc);
    if (n.body() && scb) {
      walkStmt(*n.body(), c, scb);
    }
    break;
  }
  case ast::NodeKind::NK_ProcDefn: {
    const auto &n = static_cast<const ast::ProcDefn &>(d);
    uint32_t c = ctx | toRaw(LexCtx::InProc);
    if (n.body() && scb) {
      walkStmt(*n.body(), c, scb);
    }
    break;
  }
  case ast::NodeKind::NK_StateDefn: {
    const auto &n = static_cast<const ast::StateDefn &>(d);
    uint32_t c =
        ctx | toRaw(LexCtx::InProc) | toRaw(LexCtx::InState);
    if (n.body() && scb) {
      walkStmt(*n.body(), c, scb);
    }
    break;
  }
  default:
    break;
  }
}

inline void walkUnit(const ast::CompilationUnit &cu, const DeclCb &dcb,
                     const StmtCb &scb) {
  for (const auto &item : cu.items()) {
    if (item) {
      walkDecl(*item, 0U, dcb, scb);
    }
  }
}

} // namespace nsl::sema::detail

#endif // NSL_SEMA_CONSTRAINTS_CONSTRAINT_HELPERS_H
