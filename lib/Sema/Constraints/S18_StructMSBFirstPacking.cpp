// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// lib/Sema/Constraints/S18_StructMSBFirstPacking.cpp - constructive
// S18. Spec: lang.ebnf:885 — earlier-declared struct members occupy
// more-significant bit positions of the packed layout.
//
// Per Q1 Option B the observable is `StructTypeSymbol::fields()`
// returning ArrayRef<FieldInfo> in MSB-first order with
// `fields[0].offset == totalWidth - fields[0].width`. The
// ResolutionPass already calls `setFields()` in textual order with
// LSB-first offsets; this checker re-computes the MSB-first offsets
// and re-installs them on every StructTypeSymbol it can reach.

#include "../ConstraintCheckRegistry.h"
#include "nsl/AST/CompilationUnit.h"
#include "nsl/AST/StructDecl.h"
#include "nsl/Sema/SymbolTable.h"
#include "nsl/Sema/TypeSystem.h"

#include <vector>

namespace nsl::sema {
namespace {

class S18Visitor : public ConstraintVisitor {
public:
  void run(const ConstraintContext &ctx) const override {
    if (ctx.unit == nullptr || ctx.symbols == nullptr) {
      return;
    }
    for (const auto &item : ctx.unit->items()) {
      if (!item || item->kind() != ast::NodeKind::NK_StructDecl) {
        continue;
      }
      const auto &sd = static_cast<const ast::StructDecl &>(*item);
      Symbol *sym = ctx.symbols->lookup(sd.name());
      if (sym == nullptr || sym->kind() != SymbolKind::SK_StructType) {
        continue;
      }
      auto *st = static_cast<StructTypeSymbol *>(sym);
      auto fields = st->fields();
      if (fields.empty()) {
        continue;
      }
      // Recompute MSB-first offsets: cumulative width from start;
      // offset[i] = totalWidth - cumulative_so_far - width[i].
      uint64_t total = st->totalWidth();
      std::vector<FieldInfo> msb(fields.begin(), fields.end());
      uint64_t cumulative = 0U;
      for (auto &f : msb) {
        f.offset = total - cumulative - f.width;
        cumulative += f.width;
      }
      st->setFields(std::move(msb), total);
    }
  }
};

} // namespace
} // namespace nsl::sema

NSL_REGISTER_CONSTRAINT(18, S18Visitor)
