// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// lib/Sema/SymbolTable.cpp — out-of-line definitions for the
// `Symbol` hierarchy and the `Scope` / `SymbolTable` classes
// declared in `include/nsl/Sema/SymbolTable.h`.
//
// Anchoring strategy (per `lib/AST/ASTNode.cpp` precedent): every
// abstract base's virtual destructor + every concrete subclass's
// classof helpers live here so each translation unit consuming the
// header pays no per-kind vtable cost.

#include "nsl/Sema/SymbolTable.h"

#include "llvm/ADT/StringRef.h"

#include <cassert>
#include <utility>

namespace nsl::sema {

// -----------------------------------------------------------------
// Out-of-line virtual destructors (vtable anchors)
// -----------------------------------------------------------------

Symbol::~Symbol() = default;
ValueSymbol::~ValueSymbol() = default;
ControlSymbol::~ControlSymbol() = default;

// -----------------------------------------------------------------
// SymbolKind -> string (deterministic; X-macro source order)
// -----------------------------------------------------------------

llvm::StringRef toString(SymbolKind k) {
  switch (k) {
#define NSL_SYMBOL_KIND(EnumName, ConcreteClass)                               \
  case SymbolKind::SK_##EnumName:                                              \
    return llvm::StringRef(#EnumName);
#include "nsl/Sema/SymbolKind.def"
#undef NSL_SYMBOL_KIND
  case SymbolKind::SK_count:
    break;
  }
  return llvm::StringRef("<invalid>");
}

// -----------------------------------------------------------------
// Scope
// -----------------------------------------------------------------

Symbol *Scope::lookupLocal(ast::Identifier name) const {
  auto it = table_.find(name);
  if (it == table_.end()) {
    return nullptr;
  }
  return it->second;
}

bool Scope::insert(std::unique_ptr<Symbol> sym) {
  assert(sym && "Scope::insert given null Symbol");
  ast::Identifier const name = sym->name();
  if (table_.find(name) != table_.end()) {
    // Duplicate — caller emits any "duplicate name" diagnostic;
    // Scope::insert is silent. The caller's `unique_ptr` is
    // *retained* (not moved) on the false path so the source
    // location of the failed declaration is recoverable.
    return false;
  }
  // Insert raw pointer first (DenseMap), then transfer ownership
  // into ownedSymbols_ + declOrder_. This sequencing matters
  // because `sym` is moved-from after the std::move call.
  Symbol *raw = sym.get();
  table_.insert({name, raw});
  declOrder_.push_back(raw);
  ownedSymbols_.push_back(std::move(sym));
  return true;
}

// -----------------------------------------------------------------
// SymbolTable
// -----------------------------------------------------------------

SymbolTable::SymbolTable() = default;
SymbolTable::~SymbolTable() = default;

void SymbolTable::enterScope(ScopeKind kind, Symbol *owner) {
  Scope *parent = scopes_.empty() ? nullptr : scopes_.back().get();
  scopes_.push_back(std::make_unique<Scope>(kind, parent, owner));
}

void SymbolTable::leaveScope() {
  assert(!scopes_.empty() && "leaveScope on empty stack");
  // Phase 3: retire the scope rather than destroy it. The Symbols
  // declared in this scope must outlive the resolution walk so the
  // post-Sema printer (and downstream Sn walkers / tooling) can
  // consume `Symbol*` references stored in the `ResolutionMap`
  // side-table. The retired scope is destroyed only when the
  // SymbolTable itself is destroyed.
  retiredScopes_.push_back(std::move(scopes_.back()));
  scopes_.pop_back();
}

bool SymbolTable::declare(std::unique_ptr<Symbol> sym) {
  assert(!scopes_.empty() &&
         "declare called before any enterScope; the ResolutionPass "
         "must enterScope(Global) first");
  return scopes_.back()->insert(std::move(sym));
}

Symbol *SymbolTable::lookup(ast::Identifier name) const {
  // Walk innermost-first; the first match wins per the lexical-
  // scope semantics in design §6 lines 786–793.
  for (auto it = scopes_.rbegin(); it != scopes_.rend(); ++it) {
    if (Symbol *sym = (*it)->lookupLocal(name)) {
      return sym;
    }
  }
  // Post-Sema introspection path: when the active stack is empty
  // (Sema::run finished and every scope was retired into
  // `retiredScopes_`), fall back to the retired list in reverse
  // retirement order — most-recently-retired first approximates
  // innermost-first. This makes `r.symbols->lookup("foo")` work
  // after `Sema::run()` returns, satisfying the constructive-`Sn`
  // introspection unit tests under
  // `test_unit/constructive_sn_test/`.
  if (scopes_.empty()) {
    for (auto it = retiredScopes_.rbegin(); it != retiredScopes_.rend(); ++it) {
      if (Symbol *sym = (*it)->lookupLocal(name)) {
        return sym;
      }
    }
  }
  return nullptr;
}

Symbol *SymbolTable::lookupScoped(const ast::ScopedName &name) const {
  if (name.parts.empty()) {
    return nullptr;
  }
  // Phase 2 minimum contract per data-model §2.3: single-part name
  // resolves identical to `lookup`. Multi-part SUB.port resolution
  // (which walks through `SubmoduleSymbol::templateDecl` to a
  // declare scope) lands at Phase 3 (T028) when the
  // `ResolutionPass` populates the necessary cross-references.
  if (name.parts.size() == 1) {
    return lookup(name.parts.front());
  }
  return nullptr;
}

Symbol *SymbolTable::currentModule() const {
  for (auto it = scopes_.rbegin(); it != scopes_.rend(); ++it) {
    if ((*it)->kind() == ScopeKind::Module) {
      return (*it)->owner();
    }
  }
  return nullptr;
}

const Scope *SymbolTable::currentScope() const noexcept {
  if (scopes_.empty()) {
    return nullptr;
  }
  return scopes_.back().get();
}

} // namespace nsl::sema
