<!-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception -->

# Data Model: M3 — Sema (`nsl-sema`)

**Branch**: `006-m3-sema` | **Date**: 2026-04-28
**Plan**: [plan.md](./plan.md)

This file enumerates the *types* M3 introduces to the codebase —
the `Symbol` hierarchy, the `Type` hierarchy, the `Scope` stack,
the `SemaResult` surface, and the diagnostic-bearing entities.
Definitions mirror
[`docs/design/nsl_compiler_design.md`](../../docs/design/nsl_compiler_design.md)
§§6 (lines 688–795) and §6.x (lines 797–856) verbatim per
Principle VII; this file is the plan-level summary, not a
re-derivation.

The post-condition for M3 is: every entity below has a header
slot (in `include/nsl/Sema/{Sema,SymbolTable,TypeSystem}.h`) or a
private impl-detail slot (in `lib/Sema/`), a test in `test/sema/`
or `test_unit/`, and (where applicable) a printer line in the
post-Sema `-emit=ast` output (per Q2 Option A and contract
`emit-ast-format.contract.md`).

---

## 1. Symbol hierarchy (`include/nsl/Sema/SymbolTable.h`)

### 1.1 Base classes

| Type | Header | Fields | Relationships |
|---|---|---|---|
| `Symbol` | `SymbolTable.h` | `Identifier name`; `SymbolKind kind`; `SourceRange declLoc`; `TypeRef type` | abstract base; every concrete symbol inherits transitively (per design §6 lines 692–697) |
| `ValueSymbol` | `SymbolTable.h` (nested) | (no extra fields beyond `Symbol`) | abstract mid-base; data-bearing entities (regs, wires, etc.) |
| `ControlSymbol` | `SymbolTable.h` (nested) | (no extra fields beyond `Symbol`) | abstract mid-base; `func_in` / `func_out` / `func_self` |

The `SymbolKind` enum carries one tag per concrete subclass below;
generated from a `SymbolKind.def` X-macro (analogous to M2's
`NodeKind.def`) so adding a `Symbol` subclass is a single-line
edit + an X-macro entry.

### 1.2 Value symbols (per design §6 lines 700–714)

| Type | Fields | Spec / design anchor |
|---|---|---|
| `PortSymbol` | `Direction dir ∈ {Input, Output, Inout}`; `optional<Expr*> width` | design §704; spec FR-005; `S5` direction-reversal applies at construction |
| `RegSymbol` | `optional<Expr*> init`; `optional<Expr*> width` | design §706–708; `S2`/`S23` rules apply; `S3` (`:=` LHS) consumes |
| `WireSymbol` | `optional<Expr*> width` | design §709; `S2` rejects init at construction |
| `VariableSymbol` | `optional<Expr*> width` | design §710; `S12` partial-LHS allowed |
| `IntegerSymbol` | (none beyond `Symbol`) | design §711; `S10` generate-loop-var classifier |
| `MemSymbol` | `Expr* depth`; `Expr* width`; `optional<vector<uint64_t>> initValues` | design §712–714; `S24` partial-init zero-fill resolved at construction |

### 1.3 Control symbols (per design §6 lines 716–730)

| Type | Fields | Spec / design anchor |
|---|---|---|
| `FuncInSymbol` | `vector<PortSymbol*> args`; `PortSymbol* ret` (nullable) | design §719–722; `S4` directions checked at declaration |
| `FuncOutSymbol` | `vector<PortSymbol*> args`; `PortSymbol* ret` (nullable) | design §723–726; `S4` directions checked at declaration |
| `FuncSelfSymbol` | `vector<WireSymbol*> args`; `WireSymbol* ret` (nullable) | design §727–730 |

### 1.4 Procedure / state / submodule / struct (per design §6 lines 732–741)

| Type | Fields | Spec / design anchor |
|---|---|---|
| `ProcSymbol` | `vector<RegSymbol*> argRegs` | design §732–734; `S6` arg-must-be-reg checked at declaration; `S21` proc-method classifier consumes |
| `StateSymbol` | (none beyond `Symbol`) | design §735; `S11` proc-scope checked at declaration |
| `SubmoduleSymbol` | `DeclareBlock* templateDecl` | design §736–738; `lookupScoped(SUB.port)` walks via `templateDecl` |
| `StructTypeSymbol` | `vector<FieldInfo> fields` (in MSB-first order); `uint64_t totalWidth` | design §739–741; `S18` packing resolved at construction |

`FieldInfo` is a small struct in `SymbolTable.h`:

```cpp
struct FieldInfo {
    Identifier name;
    uint64_t   width;
    uint64_t   offset;  // bit position from MSB (per S18)
};
```

### 1.5 SymbolKind enum (X-macro source-of-truth)

`SymbolTable.h` includes `#include "SymbolKind.def"` to generate the
`enum class SymbolKind`. The `.def` file:

```
NSL_SYMBOL_KIND(Port,         PortSymbol)
NSL_SYMBOL_KIND(Reg,          RegSymbol)
NSL_SYMBOL_KIND(Wire,         WireSymbol)
NSL_SYMBOL_KIND(Variable,     VariableSymbol)
NSL_SYMBOL_KIND(Integer,      IntegerSymbol)
NSL_SYMBOL_KIND(Mem,          MemSymbol)
NSL_SYMBOL_KIND(FuncIn,       FuncInSymbol)
NSL_SYMBOL_KIND(FuncOut,      FuncOutSymbol)
NSL_SYMBOL_KIND(FuncSelf,     FuncSelfSymbol)
NSL_SYMBOL_KIND(Proc,         ProcSymbol)
NSL_SYMBOL_KIND(State,        StateSymbol)
NSL_SYMBOL_KIND(Submodule,    SubmoduleSymbol)
NSL_SYMBOL_KIND(StructType,   StructTypeSymbol)
```

13 kinds. Adding a 14th is a single-line `.def` edit +
header entry (per spec SC-010).

---

## 2. Scope hierarchy (`include/nsl/Sema/SymbolTable.h`)

### 2.1 ScopeKind enum

Per design §6 lines 786–793 ("Scope stack semantics" table):

```cpp
enum class ScopeKind {
    Global,        // CompilationUnit
    Declare,       // DeclareBlock
    Module,        // ModuleBlock
    Proc,          // ProcDefn
    SeqOrParallel, // { ... } block (seq, par, alt, any inline decls)
    Function,      // FuncDefn
};
```

### 2.2 Scope class

```cpp
class Scope {
public:
    ScopeKind                              kind;
    Scope*                                 parent;     // null for Global
    Symbol*                                owner;      // the Symbol that opened this scope (e.g., ProcSymbol for Proc)
    llvm::DenseMap<llvm::StringRef, Symbol*> table;  // O(1) lookup
    std::vector<Symbol*>                   declOrder; // insertion order, for deterministic iteration
};
```

### 2.3 SymbolTable class

```cpp
class SymbolTable {
public:
    bool   declare(std::unique_ptr<Symbol>);   // returns false on duplicate name in current scope
    Symbol* lookup(Identifier);                // outward walk
    Symbol* lookupScoped(ScopedName);          // SUB.port / inst.finish / inst.field
    void   enterScope(ScopeKind, Symbol* owner = nullptr);
    void   leaveScope();
    Symbol* currentModule() const;             // walks up to nearest Module scope
private:
    std::vector<std::unique_ptr<Scope>> scopes_;  // stack
};
```

API contract: see `contracts/sema-api.contract.md`.

---

## 3. Type hierarchy (`include/nsl/Sema/TypeSystem.h`)

### 3.1 Base + concrete types

| Type | Fields | Singleton? | Design anchor |
|---|---|---|---|
| `Type` | `TypeKind kind` | abstract | §802–811 |
| `BitType` | (none beyond `Type`) | yes (`bitSingleton_`) | §848 |
| `BitVectorType` | `uint64_t width` | no (interned per width) | §813–818 |
| `StructType` | `SmallVector<FieldInfo> fields`; `uint64_t totalWidth` | no (interned per name) | §820–827 |
| `MemoryType` | `uint64_t depth`; `TypeRef element` | no (interned per `(depth, element)`) | §829–836 |
| `UnresolvedType` | (none beyond `Type`) | yes (`unresolvedSingleton_`) | M3-new (no design anchor; needed for FR-017 no-cascade) |

### 3.2 TypeKind enum

```cpp
enum class TypeKind { Bit, BitVector, Struct, Memory, Unresolved };
```

### 3.3 TypeRef alias

```cpp
using TypeRef = const Type*;  // per design §838
```

`TypeRef::nullptr` is the "no type yet" sentinel (M2 leaves
`Expr::inferredType_ == nullptr`); after `ResolutionPass`, every
`Expr::inferredType()` is either a non-null `TypeRef` or
`unresolvedSingleton()`.

### 3.4 TypeSystem class

```cpp
class TypeSystem {
public:
    TypeRef bit() const;
    TypeRef unresolved() const;
    TypeRef bitVector(uint64_t width);
    TypeRef structType(Identifier name, ArrayRef<FieldInfo> fields);
    TypeRef memory(uint64_t depth, TypeRef element);
    bool    equal(TypeRef a, TypeRef b) const noexcept { return a == b; }  // pointer equality
private:
    BitType                                                 bitSingleton_;
    UnresolvedType                                          unresolvedSingleton_;
    llvm::DenseMap<uint64_t, std::unique_ptr<BitVectorType>> bvCache_;
    llvm::DenseMap<Identifier, std::unique_ptr<StructType>> structCache_;
    llvm::DenseMap<std::pair<uint64_t, TypeRef>,
                   std::unique_ptr<MemoryType>>             memCache_;
};
```

API contract: see `contracts/sema-api.contract.md` (TypeSystem
section). Determinism contract: see
`contracts/sema-stability.contract.md` Invariant 3
("Pointer equality implies type equality").

---

## 4. Sema engine (`include/nsl/Sema/Sema.h`)

### 4.1 SemaResult

```cpp
struct SemaResult {
    std::unique_ptr<SymbolTable> symbols;     // ownership transferred from Sema
    std::unique_ptr<TypeSystem>  types;       // ownership transferred from Sema
    bool                         hasErrors;   // mirror of DiagnosticEngine::hasErrors() at run time
};
```

### 4.2 Sema class

```cpp
class Sema {
public:
    explicit Sema(DiagnosticEngine&);
    SemaResult run(CompilationUnit&);
private:
    DiagnosticEngine&         diag_;
    std::unique_ptr<SymbolTable> symbols_;
    std::unique_ptr<TypeSystem>  types_;
    void runResolutionPass(CompilationUnit&);
    void runConstraintPasses(CompilationUnit&);
};
```

`Sema::run()` is the single entry point invoked by
`Compilation::sema()` (per FR-019). It executes
`runResolutionPass` then `runConstraintPasses` in that order; on
the first stage's failure it still proceeds to the second so
multi-error reporting works.

### 4.3 Pass machinery (private, in `lib/Sema/`)

Implementation-only types not exposed in public headers:

| Type | Purpose |
|---|---|
| `ResolutionPass` (in `lib/Sema/ResolutionPass.cpp`) | the single top-down `ASTVisitor` walk that opens scopes, declares symbols, resolves names, infers widths, emits unresolved-name diagnostics |
| `ConstraintCheckRegistry` (in `lib/Sema/Sema.cpp`) | holds the 29 per-`Sn` walker entry points; each `S<NN>_*.cpp` registers itself at static-init time via `NSL_REGISTER_CONSTRAINT(SN, ...)` |
| `S<NN>Visitor` (private in each `lib/Sema/Constraints/S<NN>_*.cpp`) | per-`Sn` walker; visits only the relevant AST node kinds; emits diagnostic-on-violation |

---

## 5. Diagnostic-bearing entities

### 5.1 Frozen diagnostic strings (FR-011, FR-015)

For each error/warning `Sn` (the 23 non-constructive rows of
spec FR-010's table), the canonical message text is **frozen** at
M3 by the `s<NN>/fail.nsl` fixture's literal-string assertion. The
full table lives in `contracts/diagnostic-string.contract.md` and
is the authoritative freeze surface.

### 5.2 FixItHint instances (FR-012)

Per research §5, three (optionally four) mechanical fix-its:

| `Sn` | Fix-it shape | Replacement |
|---|---|---|
| `S3` | `replaceRange = TransferStmt::eqOpRange` | `:=` or `=` (inverse of the wrong direction) |
| `S7` | (heavier — emit error-only when not removable) | n/a unless inline |
| `S14` | `insertRange = ConditionalExpr::endRange` | ` else <expr>` template |
| `S26` (warning) | `replaceRange = function-keyword range` | `func` |

`FixItHint` is the M1 struct re-used unchanged (`SourceRange range; std::string replacement;`).

---

## 6. Symbol-state lifecycle

A `Symbol` moves through three observable states during a Sema run:

```
        declare()                       resolve_uses()
[absent] ─────────► [declared, type=Unresolved] ─────────► [declared, type=resolved]
                       (Symbol* in scope.table_;
                        type may still be Unresolved
                        if its declared-width expr
                        depends on an unresolved name)
```

- **declare()** writes the `Symbol*` into the current scope's
  `table` and `declOrder`. The `type` field is provisionally set
  from the declared width expression; if the width depends on
  an unresolved name, `type = unresolvedSingleton()` until later
  fix-up.
- **resolve_uses()** is where the `ResolutionPass` walks
  `IdentifierExpr` / `FieldAccessExpr` / `ScopedName` references
  and writes `Symbol*` back into the AST node, and where width
  fix-ups happen for forward-declared names.

After `Sema::run()` returns, every `Symbol`'s `type` is either a
real `TypeRef` (resolved successfully) OR `unresolvedSingleton()`
(at least one unresolved-name diagnostic was emitted; the rest of
Sema treats this symbol as poisoned and per-`Sn` walkers skip
subtrees rooted at it).

---

## 7. Cross-references (no pointer leaks per FR-031)

Every cross-reference between AST or Symbol entities serializes
via one of two stable keys:

- For a target with a `SourceRange`: `<file>:<line>:<col>` of
  `target.start` (per the post-Sema `-emit=ast` printer's
  `-> decl@<file>:<line>:<col>` suffix per `emit-ast-format.contract.md` Invariant 3 + 7, research §7).
- For a target without a `SourceRange` (e.g., the singleton
  `bitSingleton_` / `unresolvedSingleton_` whose `declLoc` is
  empty): a stable canonical identifier (`"<bit>"` /
  `"<unresolved>"`).

No raw `0x[0-9a-f]+` pointer values are emitted in any serialized
Sema output (printer or diagnostic). Asserted by the same regex
guard the M2 AST printer uses (per `contracts/sema-stability.contract.md`
Invariant 5).
