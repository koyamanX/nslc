<!-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception -->

# Contract: `nsl-dialect` Public API Surface

**Branch**: `007-m4-mlir-dialect` | **Date**: 2026-04-30
**Plan**: [../plan.md](../plan.md)

This contract documents the **public-header surface** of the
`nsl-dialect` library — the API that downstream layers (M5
`nsl-lower`, M6 CIRCT lowering, M7 `nsl-driver`, plus
the `nsl-opt` developer/test binary) consume. Once M4 lands, this
surface is **frozen** for the remainder of the project; any change
is treated as an M4 amendment requiring a same-PR design-doc
update (per Principle VII).

## 1. Public include path

The library exposes exactly **one** umbrella public header:

```
#include "nsl/Dialect/NSL/IR/NSLDialect.h"
```

No consumer code may include the TableGen-generated per-op headers
(`NSLOps.h.inc`, `NSLDialect.h.inc`, `NSLTypes.h.inc`) directly;
those live under `lib/Dialect/NSL/IR/` (private) and the umbrella
re-exports them. This is the single-public-header rule of
Constitution Principle II applied to `nsl-dialect` (NOT an
exception; `nsl-dialect` is not named in Principle II §3's two-
header carve-out for `nsl-ast` and `nsl-sema`).

## 2. Symbols re-exported by `NSLDialect.h`

| Symbol | C++ kind | Source |
|---|---|---|
| `nsl::dialect::NSLDialect` | class | TableGen `def NSL_Dialect : Dialect` |
| `nsl::dialect::ModuleOp` | class | TableGen `def NSL_ModuleOp` |
| `nsl::dialect::StructOp` | class | (TableGen) |
| `nsl::dialect::SubmoduleOp` | class | (TableGen) |
| `nsl::dialect::ConnectOp` | class | (TableGen) |
| `nsl::dialect::RegOp` | class | (TableGen) |
| `nsl::dialect::WireOp` | class | (TableGen) |
| `nsl::dialect::VariableOp` | class | (TableGen) |
| `nsl::dialect::MemOp` | class | (TableGen) |
| `nsl::dialect::FuncInOp` | class | (TableGen) |
| `nsl::dialect::FuncOutOp` | class | (TableGen) |
| `nsl::dialect::FuncSelfOp` | class | (TableGen) |
| `nsl::dialect::AltOp` | class | (TableGen) |
| `nsl::dialect::AnyOp` | class | (TableGen) |
| `nsl::dialect::IfOp` | class | (TableGen) |
| `nsl::dialect::ParallelOp` | class | (TableGen) |
| `nsl::dialect::SeqOp` | class | (TableGen) |
| `nsl::dialect::WhileOp` | class | (TableGen) |
| `nsl::dialect::ForOp` | class | (TableGen) |
| `nsl::dialect::CaseOp` | class | (TableGen) |
| `nsl::dialect::DefaultOp` | class | (TableGen) |
| `nsl::dialect::TransferOp` | class | (TableGen) |
| `nsl::dialect::ClockedTransferOp` | class | (TableGen) |
| `nsl::dialect::IncDecOp` | class | (TableGen) |
| `nsl::dialect::CallOp` | class | (TableGen) |
| `nsl::dialect::FinishOp` | class | (TableGen) |
| `nsl::dialect::FinishMethodOp` | class | (TableGen) |
| `nsl::dialect::InvokeMethodOp` | class | (TableGen) |
| `nsl::dialect::ProcOp` | class | (TableGen) |
| `nsl::dialect::FirstStateOp` | class | (TableGen) |
| `nsl::dialect::StateOp` | class | (TableGen) |
| `nsl::dialect::FuncOp` | class | (TableGen) |
| `nsl::dialect::GotoOp` | class | (TableGen) |
| `nsl::dialect::SimDisplayOp` | class | (TableGen) |
| `nsl::dialect::SimFinishOp` | class | (TableGen) |
| `nsl::dialect::SimInitOp` | class | (TableGen) |
| `nsl::dialect::SimDelayOp` | class | (TableGen) |
| `nsl::dialect::FireProbeOp` | class | (TableGen) |
| `nsl::dialect::StructCastOp` | class | (TableGen) |
| `nsl::dialect::FieldOp` | class | (TableGen) |
| `nsl::dialect::StructuralGenerateOp` | class | (TableGen) |
| `nsl::dialect::ModuleTerminatorOp` | class | (TableGen, auto from `SingleBlockImplicitTerminator`) |
| `nsl::dialect::ProcTerminatorOp` | class | (TableGen, auto) |
| `nsl::dialect::BitsType` | class | TableGen `def NSL_BitsType` |
| `nsl::dialect::StructType` | class | TableGen `def NSL_StructType` |
| `nsl::dialect::MemType` | class | TableGen `def NSL_MemType` |
| `nsl::dialect::registerNSLDialect` | function | hand-written |

That's 35 op classes + 2 auto-generated terminators + 3 type
classes + the dialect class + the registration function = **42
public types/functions**.

## 3. Registration entry-point contract

Signature:

```cpp
namespace nsl::dialect {
void registerNSLDialect(mlir::DialectRegistry &registry);
}
```

Behavior:

- Registers the `NSLDialect` class on the given registry.
- **Idempotent**: calling twice on the same `registry` is a no-op
  (verified by `test_unit/dialect_register_test/`).
- Thread-safe at the call site (MLIR's `DialectRegistry::insert` is
  documented thread-safe).
- Returns `void`; failures (out of memory, etc.) propagate as MLIR
  exceptions, which is the upstream convention.

## 4. Op class API surface (per op)

Each TableGen-generated op class has the standard MLIR `Op<>`
interface:

```cpp
class ModuleOp : public mlir::Op<ModuleOp, /* traits */ ...> {
public:
  static llvm::StringRef getOperationName() { return "nsl.module"; }
  static void build(mlir::OpBuilder&, mlir::OperationState&, ...);
  llvm::StringRef getSymName();         // from Symbol trait
  llvm::StringRef setSymName(llvm::StringRef);
  mlir::Region &getBody();
  mlir::Block &getBodyBlock();
  // ... per-trait accessors
  static mlir::ParseResult parse(mlir::OpAsmParser&, mlir::OperationState&);
  void print(mlir::OpAsmPrinter&);
  mlir::LogicalResult verify();
};
```

The exact accessor set is determined by the op's traits + arguments
+ regions in the `.td` record. Consumers (M5 lowering) use these
accessors via TableGen's standard generated API; consumers do NOT
dynamically construct ops by raw `Operation *` manipulation.

## 5. Type class API surface (per type)

Each type class has the standard MLIR `Type<>` interface:

```cpp
class BitsType : public mlir::Type::TypeBase<BitsType, mlir::Type, BitsTypeStorage> {
public:
  static llvm::StringRef getMnemonic() { return "bits"; }
  static BitsType get(mlir::MLIRContext *ctx, unsigned width);
  unsigned getWidth() const;
};
```

`get` is the standard interner factory; pointer equality on
`BitsType` instances implies value equality (per FR-008 + research
§5).

## 6. Stability surface

Once M4 is merged, the following are FROZEN:

1. **Op-class set**: 35 ops + 2 auto-generated terminators. Adding
   a 36th is an M4 amendment + design §7 update (per SC-012).
   Removing or renaming an op is a MAJOR version change.
2. **Op-class trait set per op**: per the data-model.md table.
   Adding a trait is a minor amendment; removing one (especially
   `Symbol` or `SymbolTable`) is MAJOR (consumers rely on the
   trait machinery).
3. **Type set**: 3 types. Same amendment rules.
4. **Op TableGen mnemonic** (`nsl.module`, `nsl.proc`, etc.): part
   of the textual IR contract; renaming is MAJOR (every fixture
   under `test/Dialect/` plus M5 lowering needs to be rewritten).
5. **`registerNSLDialect` signature**: stable.

## 7. Consumer responsibilities

- M5 `nsl-lower` SHOULD include `NSLDialect.h` once via the
  umbrella header; SHOULD NOT include any `.h.inc` directly.
- M5 `nsl-lower` SHOULD use the TableGen-generated op-builder API
  (`ModuleOp::build(...)`) rather than raw `mlir::OpBuilder::create<>()` 
  with hand-constructed `OperationState`.
- M5/M6 lowering passes SHOULD NOT mutate the dialect's TableGen
  records or hand-written verifier bodies; if a structural
  invariant turns out to be wrong, it's an M4 amendment, not an M5
  workaround.
- Tooling-track consumers (T2/T3/T6) at their respective milestones
  SHOULD continue to consume `libNSLFrontend.a` (Sema's domain) for
  symbol/type queries; they MAY consume the dialect for
  `-emit=mlir`-output inspection but MUST NOT take dialect-level
  cross-references as authoritative for source-level analyses
  (Sema is authoritative for that).
