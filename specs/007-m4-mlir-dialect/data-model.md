<!-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception -->

# Data Model: M4 — `nsl` MLIR Dialect (`nsl-dialect`)

**Branch**: `007-m4-mlir-dialect` | **Date**: 2026-04-30
**Plan**: [plan.md](./plan.md)

This file enumerates the *types* M4 introduces to the codebase —
the dialect class, the 40 op classes, the 3 type classes, and the
verifier-helper utilities. Definitions mirror
[`docs/design/nsl_compiler_design.md`](../../docs/design/nsl_compiler_design.md)
§§7–10 verbatim per Principle VII; this file is the plan-level
summary, not a re-derivation.

The post-condition for M4 is: every entity below has a TableGen
record (in `lib/Dialect/NSL/IR/NSL*.td`), a generated C++ class
(via `add_mlir_dialect`), an optional hand-written verifier body
(in `lib/Dialect/NSL/IR/NSLOps.cpp`), and a round-trip fixture (in
`test/Dialect/<category>/`) plus, where applicable, one or more
invalid fixtures.

---

## 1. Dialect class (`include/nsl/Dialect/NSL/IR/NSLDialect.h`)

| Type | Field / member | Purpose |
|---|---|---|
| `nsl::dialect::NSLDialect` | (TableGen-generated) | The MLIR dialect class. Inherits `mlir::Dialect`. Constructor calls `addOperations<...>()` and `addTypes<...>()` to register the 40 ops + 3 types. |
| `nsl::dialect::registerNSLDialect` | function | Registration entry-point. Called by `Compilation` ctor (per FR-004 / design §11 line 1145) and `nsl-opt` main (per FR-014). Idempotent (calling twice on the same `mlir::DialectRegistry` is a no-op; verified by `test_unit/dialect_register_test/`). |

The dialect's TableGen record lives in `NSLDialect.td`:

```tablegen
def NSL_Dialect : Dialect {
  let name = "nsl";
  let cppNamespace = "::nsl::dialect";
  let useDefaultTypePrinterParser = 1;
  let summary = "The NSL hardware-description IR dialect";
  let description = [{ ... brief description per design §7 lines 932–938 ... }];
}
```

A base op class `NSL_Op<string mnemonic, list<Trait> traits = []>` is
defined in `NSLDialect.td` and inherited by every op record below.

---

## 2. Op set (`lib/Dialect/NSL/IR/NSLOps.td`)

The 40 ops, in the same category-grouping as spec FR-010. Each row
notes the TableGen record name, the `nsl.*` syntactic name, the
trait set, and the verifier-implementation style (TableGen-trait-only
vs hand-written body per Q2 Option B).

### 2.1 Module-level (4 ops)

| Op | Record | Traits | Verifier style |
|---|---|---|---|
| `nsl.module` | `NSL_ModuleOp` | `Symbol`, `SymbolTable`, `SingleBlockImplicitTerminator<"ModuleTerminatorOp">`, `HasParent<"::mlir::ModuleOp">` | TableGen-trait + hand-written (`sym_name` presence; struct-field non-circularity for nested `nsl.struct` siblings) |
| `nsl.struct` | `NSL_StructOp` | `Symbol`, `HasParent<"::mlir::ModuleOp">` | TableGen-trait + hand-written (`sym_name` presence; field-list non-circular) |
| `nsl.submodule` | `NSL_SubmoduleOp` | `Symbol`, `HasParent<"NSL_ModuleOp">` | TableGen-trait only (`Symbol` machinery resolves the template ref) |
| `nsl.connect` | `NSL_ConnectOp` | `HasParent<"NSL_ModuleOp">` | TableGen-trait + hand-written (operand-type match) |

### 2.2 Storage (4 ops)

| Op | Record | Traits | Verifier style |
|---|---|---|---|
| `nsl.reg` | `NSL_RegOp` | `HasParent<"NSL_ModuleOp"/"NSL_ProcOp">` (variadic) | TableGen-trait + hand-written (result type ∈ {bits, struct}) |
| `nsl.wire` | `NSL_WireOp` | `HasParent<"NSL_ModuleOp">` | TableGen-trait + hand-written (result type = bits) |
| `nsl.variable` | `NSL_VariableOp` | `HasParent<"NSL_ModuleOp"/"NSL_FuncOp">` (variadic) | TableGen-trait + hand-written (result type = bits) |
| `nsl.mem` | `NSL_MemOp` | `HasParent<"NSL_ModuleOp">` | TableGen-trait + hand-written (result type = mem) |

### 2.3 Control terminal (3 ops)

| Op | Record | Traits | Verifier style |
|---|---|---|---|
| `nsl.func_in` | `NSL_FuncInOp` | `HasParent<"NSL_ModuleOp">` | TableGen-trait only |
| `nsl.func_out` | `NSL_FuncOutOp` | `HasParent<"NSL_ModuleOp">` | TableGen-trait only |
| `nsl.func_self` | `NSL_FuncSelfOp` | `HasParent<"NSL_ModuleOp">` | TableGen-trait only |

### 2.4 Action block (7 ops)

| Op | Record | Traits | Verifier style |
|---|---|---|---|
| `nsl.alt` | `NSL_AltOp` | one region | hand-written (≥ 1 case-or-default child; child-kind ∈ {case, default}) |
| `nsl.any` | `NSL_AnyOp` | one region | hand-written (same as alt) |
| `nsl.if` | `NSL_IfOp` | two regions | TableGen-trait only (region count is in TableGen) |
| `nsl.parallel` | `NSL_ParallelOp` | one region | TableGen-trait only |
| `nsl.seq` | `NSL_SeqOp` | one region, `HasParent<"NSL_FuncOp">` | TableGen-trait only |
| `nsl.while` | `NSL_WhileOp` | one region | hand-written (transitive parent = `NSL_SeqOp` per Q2 Option B) |
| `nsl.for` | `NSL_ForOp` | one region | hand-written (transitive parent = `NSL_SeqOp`; loop-bound attr shape) |

### 2.5 Action helper (2 ops)

| Op | Record | Traits | Verifier style |
|---|---|---|---|
| `nsl.case` | `NSL_CaseOp` | `HasParent<"NSL_AltOp"/"NSL_AnyOp">` (variadic) | TableGen-trait only |
| `nsl.default` | `NSL_DefaultOp` | `HasParent<"NSL_AltOp"/"NSL_AnyOp">` (variadic) | TableGen-trait only |

### 2.6 Atomic (7 ops)

| Op | Record | Traits | Verifier style |
|---|---|---|---|
| `nsl.transfer` | `NSL_TransferOp` | `SameOperandsElementType`, `SameOperandsShape` | TableGen-trait only |
| `nsl.clocked_transfer` | `NSL_ClockedTransferOp` | `SameOperandsElementType`, `SameOperandsShape` | TableGen-trait + hand-written (first operand is reg-like) |
| `nsl.incdec` | `NSL_IncDecOp` | enum-attr-typed | TableGen-trait + hand-written (first operand is reg-like; kind enum valid) |
| `nsl.call` | `NSL_CallOp` | `SymbolRefAttr` to control-terminal | TableGen-trait + hand-written (arg count matches resolved sym) |
| `nsl.finish` | `NSL_FinishOp` | (no traits) | hand-written (transitive parent = `NSL_ProcOp` per Q2 Option B) |
| `nsl.finish_method` | `NSL_FinishMethodOp` | `SymbolRefAttr` to `NSL_ProcOp` | TableGen-trait only |
| `nsl.invoke_method` | `NSL_InvokeMethodOp` | `SymbolRefAttr` to `NSL_ProcOp` | TableGen-trait only |

### 2.7 Procedure (4 ops)

| Op | Record | Traits | Verifier style |
|---|---|---|---|
| `nsl.proc` | `NSL_ProcOp` | `Symbol`, `SymbolTable`, `HasParent<"NSL_ModuleOp">`, `SingleBlockImplicitTerminator<"ProcTerminatorOp">` | TableGen-trait + hand-written (`sym_name` presence; ≤ 1 first_state child) |
| `nsl.first_state` | `NSL_FirstStateOp` | `HasParent<"NSL_ProcOp">`, `SymbolRefAttr` | TableGen-trait + hand-written (sym ref resolves to sibling state) |
| `nsl.state` | `NSL_StateOp` | `Symbol`, `HasParent<"NSL_ProcOp">`, one region | TableGen-trait only |
| `nsl.func` | `NSL_FuncOp` | `Symbol`, `HasParent<"NSL_ModuleOp">` | TableGen-trait only |

### 2.8 Procedure helper (1 op)

| Op | Record | Traits | Verifier style |
|---|---|---|---|
| `nsl.goto` | `NSL_GotoOp` | `SymbolRefAttr` | hand-written (transitive parent = `NSL_SeqOp` for label form OR `NSL_StateOp` for state form per Q2 Option B; sym ref resolves to a sibling label/state) |

### 2.9 System task (4 ops)

| Op | Record | Traits | Verifier style |
|---|---|---|---|
| `nsl.sim_display` | `NSL_SimDisplayOp` | `HasParent<"NSL_ModuleOp">` | TableGen-trait only |
| `nsl.sim_finish` | `NSL_SimFinishOp` | `HasParent<"NSL_ModuleOp">` | TableGen-trait only |
| `nsl.sim_init` | `NSL_SimInitOp` | `HasParent<"NSL_ModuleOp">`, one region | TableGen-trait only |
| `nsl.sim_delay` | `NSL_SimDelayOp` | int-literal cycles attr | TableGen-trait only |

### 2.10 Marker (3 ops)

| Op | Record | Traits | Verifier style |
|---|---|---|---|
| `nsl.fire_probe` | `NSL_FireProbeOp` | `SymbolRefAttr` | TableGen-trait + hand-written (sym ref resolves to a sibling control-terminal) |
| `nsl.struct_cast` | `NSL_StructCastOp` | type-match operand/result | TableGen-trait + hand-written (operand bits-width = result struct totalWidth) |
| `nsl.field` | `NSL_FieldOp` | int-attr field index | TableGen-trait + hand-written (field index in range; result type matches struct field) |

### 2.11 Expansion-only (1 op)

| Op | Record | Traits | Verifier style |
|---|---|---|---|
| `nsl.structural_generate` | `NSL_StructuralGenerateOp` | one region, loop-bound attrs | TableGen-trait + hand-written (loop-bound attr shape) |

### 2.12 Auto-generated terminators

| Op | Record | Origin |
|---|---|---|
| `nsl.module_terminator` | (auto from `SingleBlockImplicitTerminator`) | `NSL_ModuleOp` |
| `nsl.proc_terminator` | (auto from `SingleBlockImplicitTerminator`) | `NSL_ProcOp` |

Auto-generated terminators are emitted by MLIR's TableGen machinery
when the parent op uses the `SingleBlockImplicitTerminator<...>`
trait. They have no explicit `def` of their own and are not user-
visible in the printer (the parent's region prints without a
trailing terminator op when the implicit-terminator trait is in
effect). Round-trip fixtures don't test them directly; they're
implicitly exercised by their parent's round-trip fixture.

---

## 3. Type set (`lib/Dialect/NSL/IR/NSLTypes.td`)

| Type | Record | Parameters | Round-trip form |
|---|---|---|---|
| `!nsl.bits<N>` | `NSL_BitsType` | `unsigned width` | `!nsl.bits<8>` |
| `!nsl.struct<@T>` | `NSL_StructType` | `mlir::SymbolRefAttr name` | `!nsl.struct<@MyStruct>` |
| `!nsl.mem<[D x T]>` | `NSL_MemType` | `unsigned depth`, `mlir::Type elementType` | `!nsl.mem<[256 x !nsl.bits<8>]>` |

Default printer/parser via `useDefaultTypePrinterParser = 1` on the
dialect (per FR-008 + research §5).

---

## 4. Verifier helper utilities (`lib/Dialect/NSL/IR/NSLOps.cpp`, anonymous namespace)

| Helper | Signature | Purpose |
|---|---|---|
| `findAncestorOfKind<T>` | `template <typename T> T findAncestorOfKind(mlir::Operation *op);` | Walks `op->getParentOp()` upward; returns the first ancestor that is-a `T` or `nullptr`. Used by ~5 transitive-parent verifiers per Q2 Option B. |
| `emitParentMismatch` | `mlir::LogicalResult emitParentMismatch(mlir::Operation *op, llvm::StringRef expectedKind);` | Emits the standard "must be enclosed by 'nsl.<kind>'" diagnostic and returns `failure()`. Reused across the ~5 transitive-parent ops. |
| `isRegLikeValue` | `bool isRegLikeValue(mlir::Value v);` | Checks whether `v` is the result of an `nsl.reg` op or an `nsl.field` op whose source is a struct-typed reg. Used by `nsl.clocked_transfer`, `nsl.incdec`. |

These helpers are compile-unit-local — no public-header surface.

---

## 5. Driver dialect-load surface (`lib/Driver/Compilation.cpp` / `LowerToNSL.cpp` / `RunNSLPasses.cpp`)

| Entity | Header / source | Role at M4 |
|---|---|---|
| `Compilation::lowerToNSL(ast::CompilationUnit&, sema::SemaResult&)` | `lib/Driver/LowerToNSL.cpp` | **STUB at M4** — emits diagnostic "MLIR lowering not yet implemented; see M5" and returns empty `mlir::OwningOpRef<mlir::ModuleOp>`. Body lands at M5. |
| `Compilation::runNSLPasses(mlir::ModuleOp)` | `lib/Driver/RunNSLPasses.cpp` | **STUB at M4** — same diagnostic + returns `mlir::failure()`. Body lands at M5. |
| `Compilation::Compilation(CompileOptions)` | `lib/Driver/Compilation.cpp` | **MODIFIED at M4** — gains the line `mlirCtx_.loadDialect<nsl::dialect::NSLDialect>()` per design §11 line 1145. |

The stubs are reachable only via `lowerToNSL`'s C++ caller; FR-023
ensures the `nslc` CLI never reaches them at M4 (the `-emit=*` parser
rejects `mlir`/`hw`/`verilog` choices).

---

## 6. `nsl-opt` developer/test binary (`tools/nsl-opt/main.cpp`)

| Entity | Source | Role at M4 |
|---|---|---|
| `int main(int argc, char **argv)` | `tools/nsl-opt/main.cpp` | Calls `mlir::asMainReturnCode(mlir::MlirOptMain(...))` after building a `mlir::DialectRegistry` populated via `nsl::dialect::registerNSLDialect(registry)` and `registry.insert<HW, Comb, Seq, FSM, SV>()`. Zero passes registered (per FR-015). |

Total: ~50 lines of C++ in one source file.

---

## 7. Fixture inventory (`test/Dialect/`, `test_unit/dialect_register_test/`)

| Path | Count | Per FR | Purpose |
|---|---|---|---|
| `test/Dialect/<category>/<op>_roundtrip.mlir` | 40 | FR-017 | Per-op round-trip pass fixture |
| `test/Dialect/Types/<type>_roundtrip.mlir` | 3 | FR-018 | Per-type round-trip pass fixture |
| `test/Dialect/<category>/<op>_invalid_<reason>.mlir` | ~50 | FR-019 | Per-invariant invalid-rejection fixture |
| `test_unit/dialect_register_test/register_idempotency_test.cc` | 1 | (research §1) | gtest: `registerNSLDialect()` × 2 = no-op |

Total: **~88 lit fixtures + 1 gtest** at M4.

---

## 8. Fixture-coverage CI guard data (`.specify/m4_invariant_table.json`)

A small data file generated from the spec FR-013 invariant table to
let `scripts/check_dialect_coverage.py` (per research §9) verify
fixture existence without parsing Markdown. Schema:

```json
{
  "ops": [
    { "name": "nsl.module",
      "category": "module-level",
      "invariants": ["parent-builtin-module", "sym-name-required"] },
    { "name": "nsl.seq",
      "category": "action-block",
      "invariants": ["parent-func"] },
    ...
  ]
}
```

The file lives under `.specify/` (alongside `feature.json`,
`init-options.json`, etc.) so its lifecycle tracks the spec-author
workflow rather than the source tree's per-feature directories. CI
asserts `.specify/m4_invariant_table.json` is in sync with FR-013
before running coverage checks.

---

## 9. Header-surface invariant

The single public umbrella `include/nsl/Dialect/NSL/IR/NSLDialect.h`
re-exports:

- The dialect class `nsl::dialect::NSLDialect` (via
  `#include "NSLDialect.h.inc"`).
- The op classes (`nsl::dialect::ModuleOp`,
  `nsl::dialect::ProcOp`, …) via `#include "NSLOps.h.inc"`.
- The type classes (`nsl::dialect::BitsType`,
  `nsl::dialect::StructType`, `nsl::dialect::MemType`) via
  `#include "NSLTypes.h.inc"`.
- The `registerNSLDialect(mlir::DialectRegistry&)` function
  declaration.

Consumers (`nsl-opt`, `nsl-driver`) `#include
"nsl/Dialect/NSL/IR/NSLDialect.h"` ONLY — never the per-op `.inc`
headers directly. This is the Principle II §3 single-public-header
rule applied to `nsl-dialect` (NOT a constitutional carve-out;
unlike `nsl-ast` and `nsl-sema` which have explicit Principle II §3
exceptions).
