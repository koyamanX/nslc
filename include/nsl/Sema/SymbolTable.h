// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// include/nsl/Sema/SymbolTable.h — `Symbol` hierarchy + `Scope` stack
// + `SymbolTable` interface.
//
// Public surface (per `contracts/sema-api.contract.md` Invariant 1):
// this header is one of three permitted public umbrella headers under
// `include/nsl/Sema/` (Constitution Principle II §3 amended at
// v1.6.0). The other two are `Sema.h` and `TypeSystem.h`.
//
// Per the same constitutional amendment, every concrete `Symbol`
// subclass nests in this header — NOT in a separate per-kind header.
// The `SymbolKind.def` X-macro (sibling file) is the single source of
// truth for the kind enum and the per-kind concrete-class names.
//
// Mirrors `docs/design/nsl_compiler_design.md` §6 lines 688–795
// verbatim per Constitution Principle VII (spec/design coupling).

#ifndef NSL_SEMA_SYMBOL_TABLE_H
#define NSL_SEMA_SYMBOL_TABLE_H

#include "nsl/AST/ASTNode.h"
#include "nsl/Basic/SourceLocation.h"
#include "nsl/Sema/TypeSystem.h"

#include "llvm/ADT/ArrayRef.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/StringRef.h"

#include <cstdint>
#include <memory>
#include <vector>

namespace nsl::ast {
class DeclareBlock;
} // namespace nsl::ast

namespace nsl::sema {

// Forward declarations of every concrete subclass — needed because
// the abstract bases below are emitted before the concrete bodies
// and certain helper types (e.g., `FuncInSymbol::args`) must refer
// to other concrete `Symbol*` types.
#define NSL_SYMBOL_KIND(EnumName, ConcreteClass) class ConcreteClass;
#include "nsl/Sema/SymbolKind.def"
#undef NSL_SYMBOL_KIND

/// Closed set of concrete `Symbol` kinds (data-model §1.5).
///
/// Enumerator names are `SK_<EnumName>` where `<EnumName>` matches
/// the C++ class name minus the `Symbol` suffix (e.g.,
/// `SK_Port` corresponds to `PortSymbol`). Source order of
/// `SymbolKind.def` IS the enum order (Principle V determinism).
enum class SymbolKind : uint8_t {
#define NSL_SYMBOL_KIND(EnumName, ConcreteClass) SK_##EnumName,
#include "nsl/Sema/SymbolKind.def"
#undef NSL_SYMBOL_KIND

  /// Sentinel for fixed-size tables keyed by kind. Always last.
  SK_count
};

/// Convert a `SymbolKind` to its enumerator name (e.g., `"Port"`,
/// `"FuncIn"`). Stable across patches (`sema-stability.contract.md`
/// Invariant 4 by analogy with the AST printer's invariant). The
/// returned string is NOT prefixed with `SK_`.
llvm::StringRef toString(SymbolKind k);

// `FieldInfo` is defined in `TypeSystem.h` (which `SymbolTable.h`
// already includes) — see the comment there for why it lives in
// the type-system header rather than this one. Both `StructType`
// and `StructTypeSymbol` consume the same record.

// -----------------------------------------------------------------
// Symbol — abstract base
// -----------------------------------------------------------------

/// Abstract base of every concrete `Symbol` kind. Carries the
/// runtime `SymbolKind` discriminator plus the four fields that
/// every kind shares (`name`, `kind`, `declLoc`, `type`) per
/// design §6 lines 692–697.
///
/// Construction: every concrete `Symbol` MUST be constructed with a
/// non-empty `SourceRange` so post-Sema `-emit=ast` can render the
/// `→ decl@<file>:<line>:<col>` decoration deterministically.
///
/// Lifetime: `Symbol`s are owned by their declaring `Scope` via
/// `std::unique_ptr<Symbol>` inside `SymbolTable::declare()`. Cross-
/// references between symbols are non-owning raw pointers (see
/// `FuncInSymbol::args` etc.); these point into the same
/// `SymbolTable` instance and have its lifetime.
class Symbol {
public:
  Symbol() = delete;

  /// Out-of-line virtual destructor (anchored in `SymbolTable.cpp`)
  /// so RTTI / vtable have a single home and per-kind translation
  /// units do not duplicate it.
  virtual ~Symbol();

  Symbol(const Symbol &)            = delete;
  Symbol &operator=(const Symbol &) = delete;
  Symbol(Symbol &&)                 = delete;
  Symbol &operator=(Symbol &&)      = delete;

  /// Runtime kind discriminator (closed set; one per
  /// `SymbolKind.def` entry).
  [[nodiscard]] SymbolKind kind() const noexcept { return kind_; }

  /// User-source identifier — non-owning view into the declaring
  /// `Buffer`'s bytes. Lifetime equals the surrounding compilation's
  /// `SourceManager`.
  [[nodiscard]] ast::Identifier name() const noexcept { return name_; }

  /// `SourceRange` of the declaration site — first-token-start to
  /// last-token-end of the declaration construct (Principle IV).
  [[nodiscard]] SourceRange declLoc() const noexcept { return declLoc_; }

  /// Resolved `TypeRef` of this symbol's value (the bit-width / type
  /// the symbol carries). Valid post-`Sema::run()`. May be
  /// `TypeSystem::unresolved()` when the declaration's width
  /// expression contains an unresolved name (FR-017 no-cascade).
  [[nodiscard]] TypeRef type() const noexcept { return type_; }

  /// Set the resolved `TypeRef`. Called by the `ResolutionPass` once
  /// width inference has determined the kind's bit-width. Phase 2
  /// stub: the public mutator exists so `ResolutionPass` (Phase 3)
  /// can wire it without re-publishing the header.
  void setType(TypeRef t) noexcept { type_ = t; }

protected:
  /// Concrete subclasses forward `(SymbolKind::SK_<Name>, name,
  /// declLoc)` here; `type` is initially null and filled by the
  /// `ResolutionPass`.
  Symbol(SymbolKind k, ast::Identifier n, SourceRange r) noexcept
      : kind_(k), name_(n), declLoc_(r), type_(nullptr) {}

private:
  SymbolKind       kind_;
  ast::Identifier  name_;
  SourceRange      declLoc_;
  TypeRef          type_;
};

// -----------------------------------------------------------------
// ValueSymbol / ControlSymbol — abstract mid-bases
// -----------------------------------------------------------------

/// Abstract mid-base for data-bearing entities: ports, regs, wires,
/// variables, integers, mems. No extra fields beyond `Symbol`; the
/// distinction matters for per-`Sn` walkers that operate uniformly
/// on "value-shaped" declarations (e.g., `S3` LHS-direction).
class ValueSymbol : public Symbol {
public:
  ~ValueSymbol() override;

  ValueSymbol(const ValueSymbol &)            = delete;
  ValueSymbol &operator=(const ValueSymbol &) = delete;
  ValueSymbol(ValueSymbol &&)                 = delete;
  ValueSymbol &operator=(ValueSymbol &&)      = delete;

protected:
  ValueSymbol(SymbolKind k, ast::Identifier n, SourceRange r) noexcept
      : Symbol(k, n, r) {}
};

/// Abstract mid-base for control-flow terminals: `func_in`,
/// `func_out`, `func_self`. No extra fields beyond `Symbol`; mid-
/// base lets per-`Sn` walkers switch cleanly between the value-
/// shaped and control-shaped families (e.g., `S4`/`S5` direction
/// rules apply only to `ControlSymbol`s).
class ControlSymbol : public Symbol {
public:
  ~ControlSymbol() override;

  ControlSymbol(const ControlSymbol &)            = delete;
  ControlSymbol &operator=(const ControlSymbol &) = delete;
  ControlSymbol(ControlSymbol &&)                 = delete;
  ControlSymbol &operator=(ControlSymbol &&)      = delete;

protected:
  ControlSymbol(SymbolKind k, ast::Identifier n, SourceRange r) noexcept
      : Symbol(k, n, r) {}
};

// -----------------------------------------------------------------
// Per-kind concrete subclasses (data-model §§1.2–1.4)
// -----------------------------------------------------------------

/// Direction of a `port` declaration (per design §6 line 704). The
/// `S5` return-terminal-direction rule is enforced at the
/// `FuncIn`/`FuncOut`/`FuncSelf` declaration site, not here.
enum class PortDirection : uint8_t { Input, Output, Inout };

/// `port` declaration in a `declare` block (data-model §1.2).
/// Optional `width` is the parser's width expression; the resolved
/// width lives in the inherited `Symbol::type()` after Sema runs.
class PortSymbol final : public ValueSymbol {
public:
  PortSymbol(ast::Identifier n, SourceRange r, PortDirection dir) noexcept
      : ValueSymbol(SymbolKind::SK_Port, n, r), dir_(dir) {}

  /// Direction of this port (Input / Output / Inout) per the
  /// `declare`-block syntax.
  [[nodiscard]] PortDirection direction() const noexcept { return dir_; }

  static constexpr SymbolKind kKind = SymbolKind::SK_Port;
  static bool classof(const Symbol *s) noexcept { return s->kind() == kKind; }

private:
  PortDirection dir_;
};

/// `reg` declaration (data-model §1.2). The `S2`/`S23` rules apply
/// at construction time inside the `ResolutionPass`.
class RegSymbol final : public ValueSymbol {
public:
  RegSymbol(ast::Identifier n, SourceRange r) noexcept
      : ValueSymbol(SymbolKind::SK_Reg, n, r) {}

  static constexpr SymbolKind kKind = SymbolKind::SK_Reg;
  static bool classof(const Symbol *s) noexcept { return s->kind() == kKind; }
};

/// `wire` declaration (data-model §1.2). `S2` rejects an init at
/// construction time inside the `ResolutionPass`.
class WireSymbol final : public ValueSymbol {
public:
  WireSymbol(ast::Identifier n, SourceRange r) noexcept
      : ValueSymbol(SymbolKind::SK_Wire, n, r) {}

  static constexpr SymbolKind kKind = SymbolKind::SK_Wire;
  static bool classof(const Symbol *s) noexcept { return s->kind() == kKind; }
};

/// `variable` declaration (data-model §1.2). `S12` partial-LHS
/// allowed; checked by the `S12` walker.
class VariableSymbol final : public ValueSymbol {
public:
  VariableSymbol(ast::Identifier n, SourceRange r) noexcept
      : ValueSymbol(SymbolKind::SK_Variable, n, r) {}

  static constexpr SymbolKind kKind = SymbolKind::SK_Variable;
  static bool classof(const Symbol *s) noexcept { return s->kind() == kKind; }
};

/// `integer` declaration (data-model §1.2). The `S10` generate-loop-
/// variable classifier checks against this kind.
class IntegerSymbol final : public ValueSymbol {
public:
  IntegerSymbol(ast::Identifier n, SourceRange r) noexcept
      : ValueSymbol(SymbolKind::SK_Integer, n, r) {}

  static constexpr SymbolKind kKind = SymbolKind::SK_Integer;
  static bool classof(const Symbol *s) noexcept { return s->kind() == kKind; }
};

/// `mem` declaration (data-model §1.2; design §6 lines 712–714).
/// `S24` partial-init zero-fill is resolved at construction time.
class MemSymbol final : public ValueSymbol {
public:
  MemSymbol(ast::Identifier n, SourceRange r) noexcept
      : ValueSymbol(SymbolKind::SK_Mem, n, r) {}

  static constexpr SymbolKind kKind = SymbolKind::SK_Mem;
  static bool classof(const Symbol *s) noexcept { return s->kind() == kKind; }
};

/// `func_in` declaration (data-model §1.3; design §6 lines 719–722).
/// `S4` direction-rules are enforced at the `ResolutionPass` site
/// when populating `args` / `ret`.
class FuncInSymbol final : public ControlSymbol {
public:
  FuncInSymbol(ast::Identifier n, SourceRange r) noexcept
      : ControlSymbol(SymbolKind::SK_FuncIn, n, r) {}

  static constexpr SymbolKind kKind = SymbolKind::SK_FuncIn;
  static bool classof(const Symbol *s) noexcept { return s->kind() == kKind; }
};

/// `func_out` declaration (data-model §1.3; design §6 lines 723–726).
class FuncOutSymbol final : public ControlSymbol {
public:
  FuncOutSymbol(ast::Identifier n, SourceRange r) noexcept
      : ControlSymbol(SymbolKind::SK_FuncOut, n, r) {}

  static constexpr SymbolKind kKind = SymbolKind::SK_FuncOut;
  static bool classof(const Symbol *s) noexcept { return s->kind() == kKind; }
};

/// `func_self` declaration (data-model §1.3; design §6 lines 727–730).
class FuncSelfSymbol final : public ControlSymbol {
public:
  FuncSelfSymbol(ast::Identifier n, SourceRange r) noexcept
      : ControlSymbol(SymbolKind::SK_FuncSelf, n, r) {}

  static constexpr SymbolKind kKind = SymbolKind::SK_FuncSelf;
  static bool classof(const Symbol *s) noexcept { return s->kind() == kKind; }
};

/// `proc_name` declaration (data-model §1.4; design §6 lines 732–734).
/// `S6` arg-must-be-reg rule is enforced at the `ResolutionPass`
/// site; `S21` proc-method classifier consumes this kind.
class ProcSymbol final : public Symbol {
public:
  ProcSymbol(ast::Identifier n, SourceRange r) noexcept
      : Symbol(SymbolKind::SK_Proc, n, r) {}

  static constexpr SymbolKind kKind = SymbolKind::SK_Proc;
  static bool classof(const Symbol *s) noexcept { return s->kind() == kKind; }
};

/// `state_name` declaration (data-model §1.4; design §6 line 735).
/// `S11` proc-scope rule is enforced at declaration site.
class StateSymbol final : public Symbol {
public:
  StateSymbol(ast::Identifier n, SourceRange r) noexcept
      : Symbol(SymbolKind::SK_State, n, r) {}

  static constexpr SymbolKind kKind = SymbolKind::SK_State;
  static bool classof(const Symbol *s) noexcept { return s->kind() == kKind; }
};

/// Submodule instance declaration (data-model §1.4; design §6 lines
/// 736–738). `lookupScoped(SUB.port)` walks via `templateDecl` to
/// find the port symbol in the submodule's `declare` scope.
class SubmoduleSymbol final : public Symbol {
public:
  SubmoduleSymbol(ast::Identifier n, SourceRange r,
                  const ast::DeclareBlock *templateDecl) noexcept
      : Symbol(SymbolKind::SK_Submodule, n, r), templateDecl_(templateDecl) {}

  /// The `declare`-block of the submodule's template — the head of
  /// the scope into which `lookupScoped` recurses for `SUB.port`.
  /// May be null if the submodule references an unresolved
  /// declare-block; downstream `Sn` walkers MUST tolerate this
  /// (FR-017 no-cascade).
  [[nodiscard]] const ast::DeclareBlock *templateDecl() const noexcept {
    return templateDecl_;
  }

  static constexpr SymbolKind kKind = SymbolKind::SK_Submodule;
  static bool classof(const Symbol *s) noexcept { return s->kind() == kKind; }

private:
  const ast::DeclareBlock *templateDecl_;
};

/// `struct` type declaration (data-model §1.4; design §6 lines
/// 739–741). `S18` MSB-first packing is resolved at construction
/// time; the `fields()` introspection method is the public surface
/// for the `S18` constructive test (per Q1 Option B; research §6).
class StructTypeSymbol final : public Symbol {
public:
  StructTypeSymbol(ast::Identifier n, SourceRange r) noexcept
      : Symbol(SymbolKind::SK_StructType, n, r), totalWidth_(0) {}

  /// MSB-first ordered field records. Phase 2 ships the empty
  /// vector; the `S18` walker (Phase 4) populates this in
  /// declaration order with MSB-first offsets.
  [[nodiscard]] llvm::ArrayRef<FieldInfo> fields() const noexcept {
    return fields_;
  }

  /// Total bit-width of the packed struct (sum of `fields[i].width`).
  [[nodiscard]] uint64_t totalWidth() const noexcept { return totalWidth_; }

  /// Phase 4 mutator used by the `S18` walker.
  void setFields(std::vector<FieldInfo> f, uint64_t total) noexcept {
    fields_     = std::move(f);
    totalWidth_ = total;
  }

  static constexpr SymbolKind kKind = SymbolKind::SK_StructType;
  static bool classof(const Symbol *s) noexcept { return s->kind() == kKind; }

private:
  std::vector<FieldInfo> fields_;
  uint64_t               totalWidth_;
};

// -----------------------------------------------------------------
// Scope (data-model §2)
// -----------------------------------------------------------------

/// The six scope kinds that can appear on the resolution stack
/// (data-model §2.1; design §6 lines 786–793).
enum class ScopeKind : uint8_t {
  Global,        ///< `CompilationUnit`
  Declare,       ///< `DeclareBlock`
  Module,        ///< `ModuleBlock`
  Proc,          ///< `ProcDefn`
  SeqOrParallel, ///< `seq`/`par`/`alt`/`any` inline declarations
  Function,      ///< `FuncDefn`
};

/// Single frame on the lexical-scope stack. Contains the symbol
/// table for this scope and a pointer back to the parent. Owned by
/// the surrounding `SymbolTable` via `std::unique_ptr<Scope>` (per
/// research §2: avoids the recursive-iterator-invalidation pitfalls
/// of a flat vector under nested scope creation).
class Scope {
public:
  Scope(ScopeKind k, Scope *parent, Symbol *owner) noexcept
      : kind_(k), parent_(parent), owner_(owner) {}

  /// Kind of this scope (Global / Declare / Module / Proc /
  /// SeqOrParallel / Function).
  [[nodiscard]] ScopeKind kind() const noexcept { return kind_; }

  /// Enclosing scope, or null for the `Global` root.
  [[nodiscard]] Scope *parent() const noexcept { return parent_; }

  /// The `Symbol` that opened this scope (e.g., the `ProcSymbol`
  /// for a `Proc` scope; the `StructTypeSymbol` for a `Declare`
  /// scope of a struct). May be null for `Global`/`Module`.
  [[nodiscard]] Symbol *owner() const noexcept { return owner_; }

  /// Look up `name` in this scope only (no outward walk). Returns
  /// null if not present. O(1) via `DenseMap`.
  [[nodiscard]] Symbol *lookupLocal(ast::Identifier name) const;

  /// Insert a `Symbol` into this scope. Returns false if a symbol
  /// with the same name already exists (caller's responsibility to
  /// emit any "duplicate name" diagnostic — `Scope::insert` itself
  /// is silent). Otherwise transfers ownership and appends to
  /// `declOrder` (insertion order, for deterministic iteration).
  bool insert(std::unique_ptr<Symbol> sym);

  /// Iteration-ordered view of every `Symbol` declared in this
  /// scope. Order matches insertion (FR-030 / Invariant 2). The
  /// pointers are non-owning; ownership lives in `ownedSymbols_`.
  [[nodiscard]] llvm::ArrayRef<Symbol *> declOrder() const noexcept {
    return declOrder_;
  }

private:
  ScopeKind                                       kind_;
  Scope                                          *parent_;
  Symbol                                         *owner_;
  llvm::DenseMap<llvm::StringRef, Symbol *>       table_;
  std::vector<std::unique_ptr<Symbol>>            ownedSymbols_;
  std::vector<Symbol *>                           declOrder_;
};

// -----------------------------------------------------------------
// SymbolTable (data-model §2.3)
// -----------------------------------------------------------------

/// The lexical-scope stack manager. Owned by `SemaResult::symbols`
/// after `Sema::run()` returns (per `sema-api.contract.md` Invariant
/// 6). Consumers (the post-Sema `-emit=ast` printer; the M4+ later
/// stages; the T-track tooling libraries) read through this surface
/// and never mutate it.
///
/// Construction: `SymbolTable` starts empty. `enterScope(Global,
/// nullptr)` is the conventional first call (`ResolutionPass` does
/// this when it visits `CompilationUnit`).
class SymbolTable {
public:
  SymbolTable();
  ~SymbolTable();

  SymbolTable(const SymbolTable &)            = delete;
  SymbolTable &operator=(const SymbolTable &) = delete;
  SymbolTable(SymbolTable &&)                 = delete;
  SymbolTable &operator=(SymbolTable &&)      = delete;

  /// Push a new scope onto the stack. `owner` is the `Symbol*` that
  /// "opened" this scope (e.g., the `ProcSymbol` for a `Proc`
  /// scope; null for `Global` / `Module`). The caller is responsible
  /// for matching `enterScope` and `leaveScope` calls; mismatches
  /// are an internal-logic bug, not a diagnostic-bearing event.
  void enterScope(ScopeKind kind, Symbol *owner = nullptr);

  /// Pop the top scope off the stack. Asserts (debug builds) that
  /// the stack is non-empty.
  void leaveScope();

  /// Declare a new `Symbol` in the current scope. Returns `false`
  /// if the name is already declared in the *current* scope (no
  /// outward walk — shadowing across scopes is permitted; same-
  /// scope duplicates are not). On success, ownership transfers
  /// into the scope and a non-owning `Symbol*` is the return
  /// value's complement (extracted via `lookup` if needed).
  ///
  /// The caller emits any "duplicate name" diagnostic on the false
  /// return; `declare` itself is silent.
  bool declare(std::unique_ptr<Symbol> sym);

  /// Look up `name` starting in the current scope and walking
  /// outward to the enclosing scopes. Returns the innermost match
  /// or null if no scope on the stack has the name.
  ///
  /// Per `sema-stability.contract.md` Invariant 8: the returned
  /// `Symbol*` is owned by `*this` and MUST NOT be passed to or
  /// compared with a `Symbol*` from a different `SymbolTable`
  /// instance.
  [[nodiscard]] Symbol *lookup(ast::Identifier name) const;

  /// Look up a dotted scoped name (e.g., `SUB.port`, `inst.finish`).
  /// Two-step (per design §6 lines 794–795): first calls
  /// `lookup(parts[0])` for the head, validates the resolved kind,
  /// then re-enters the head's target scope to look up the tail.
  ///
  /// Phase 2 ships the public-API entry point only; the head-kind
  /// validation logic lands with the `ResolutionPass` in Phase 3.
  /// At Phase 2 the implementation is `lookup(parts[0])` for
  /// 1-part names and null for multi-part names — sufficient for
  /// the scaffolding tests; will be replaced wholesale in Phase 3
  /// with no public-API change.
  [[nodiscard]] Symbol *lookupScoped(const ast::ScopedName &name) const;

  /// The `Symbol*` that owns the nearest enclosing `Module` scope,
  /// or null if no `Module` scope is currently on the stack.
  /// Returns the module owner directly (typically a synthetic
  /// `Symbol*` for the `ModuleBlock` that the `ResolutionPass`
  /// stamps on enterScope).
  [[nodiscard]] Symbol *currentModule() const;

  /// Access the topmost scope on the stack (read-only). Returns
  /// null only on a fresh `SymbolTable` before any `enterScope`
  /// call. Used by `ResolutionPass` for boundary checks; tooling
  /// may use it for incremental introspection.
  [[nodiscard]] const Scope *currentScope() const noexcept;

  /// Number of scopes currently on the stack. 0 before the first
  /// `enterScope`; matches AST nesting depth during `ResolutionPass`
  /// per the test contract in `test_unit/symbol_table_test/`.
  [[nodiscard]] std::size_t scopeDepth() const noexcept {
    return scopes_.size();
  }

private:
  std::vector<std::unique_ptr<Scope>> scopes_;
};

} // namespace nsl::sema

#endif // NSL_SEMA_SYMBOL_TABLE_H
