<!-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception -->

# Contract: `nsl-lower` Public API Surface

**Branch**: `008-m5-structural-passes` | **Date**: 2026-04-30
**Spec**: [spec.md](../spec.md) FR-001, FR-004, FR-005, FR-011, FR-020, FR-021

This contract freezes the public surface that `Lower.h` exports.
Any change to this surface after M5 freeze is a contract amendment
and MUST update this file in the same patch (Principle VII).

---

## 1. Public umbrella header

**File**: `include/nsl/Lower/Lower.h`
**Library**: `nsl-lower` (layer 8a)
**Consumers**: `nsl-driver` (`lib/Driver/Compilation.cpp`,
`lib/Driver/LowerToNSL.cpp`, `lib/Driver/RunNSLPasses.cpp`),
`tools/nsl-opt/main.cpp`.

This is the single public header for `nsl-lower`. Per Constitution
Principle II, `nsl-lower` is NOT one of the named exceptions
(`nsl-ast`, `nsl-sema`) — it gets exactly one public header.

---

## 2. Frozen public surface (the M5 freeze list)

The following symbols + signatures are frozen at M5 acceptance.
Any addition / removal / signature change is a contract amendment.

### 2.1 Visitor entry point (1 free function)

```cpp
namespace nsl::lower {

mlir::OwningOpRef<mlir::ModuleOp>
astToMLIR(mlir::MLIRContext& ctx,
          const ast::CompilationUnit& cu,
          const sema::SemaResult& sr);

} // namespace nsl::lower
```

**Behaviour contract**:

- Returns a non-null `OwningOpRef` on success.
- Returns a `nullptr`-equivalent (failed) `OwningOpRef` on internal
  invariant violation; in this case at least one error has been
  posted to the active `mlir::DiagnosticEngine` (the
  `DiagnosticBridge` in `Compilation::lowerToNSL` then forwards it
  to `basic::DiagnosticEngine`).
- Does NOT enforce `sr.hasErrors() == false`. The caller is
  responsible for that gate (see FR-020 / `Compilation::lowerToNSL`).
- Walks `cu` exactly once (Q4 → Option A; research §4).
- Borrows `ctx`, `cu`, `sr` by reference; does not extend their
  lifetime past the call.
- Every emitted MLIR op carries `mlir::Location` resolvable to the
  AST `SourceRange` (FR-008).

### 2.2 Pass-construction free functions (6 free functions)

```cpp
namespace nsl::lower {

std::unique_ptr<mlir::Pass> createNSLResolveParamsPass();
std::unique_ptr<mlir::Pass> createNSLExpandGeneratePass();
std::unique_ptr<mlir::Pass> createNSLExpandVariablesPass();
std::unique_ptr<mlir::Pass> createNSLExplodeSubmodArrayPass();
std::unique_ptr<mlir::Pass> createNSLInlineInternalFuncPass();
std::unique_ptr<mlir::Pass> createNSLCheckSemanticsPass();

} // namespace nsl::lower
```

**Behaviour contract** (per pass):

| Function | CLI flag | `runOnOperation()` post-condition |
|---|---|---|
| `createNSLResolveParamsPass` | `-nsl-resolve-params` | zero unresolved `nsl.param_int` / `nsl.param_str` operand refs in the input `nsl::ModuleOp` |
| `createNSLExpandGeneratePass` | `-nsl-expand-generate` | zero `nsl.structural_generate` ops |
| `createNSLExpandVariablesPass` | `-nsl-expand-variables` | zero `nsl.variable` ops |
| `createNSLExplodeSubmodArrayPass` | `-nsl-explode-submod-array` | zero array-form `nsl.submodule` ops |
| `createNSLInlineInternalFuncPass` | `-nsl-inline-internal-func` | IR byte-identical to input (M5 no-op slot per Q3 → Option B) |
| `createNSLCheckSemanticsPass` | `-nsl-check-semantics` | zero diagnostic emissions on clean input; one diagnostic per residue / sensitive-`Sn` violation otherwise |

**Universal pass invariants**:

- Each pass is `mlir::OperationPass<nsl::dialect::ModuleOp>`.
- Each pass returns `mlir::failure()` only on UNRECOVERABLE
  internal invariant violations; semantic-rejection diagnostics
  (residue, S-violations) cause `runOnOperation` to call
  `signalPassFailure()` which propagates to the surrounding
  `PassManager::run()` failure.
- Each pass calls `op->emitError(...)` / `op->emitOpError(...)`
  for diagnostics — these flow through `DiagnosticBridge` to
  `basic::DiagnosticEngine` per FR-019.
- Each pass MUST be deterministic per Principle V + research §13
  (no pointer-keyed iteration, no time/PID sources).

### 2.3 Pass-registration helper (1 free function)

```cpp
namespace nsl::lower {

void registerNSLLowerPasses();

} // namespace nsl::lower
```

**Behaviour contract**:

- Calls `mlir::registerPass(create<X>Pass)` for each of the six
  passes from §2.2 in any order.
- Idempotent — calling twice is safe (MLIR's registry is
  idempotent by design).
- Called from `tools/nsl-opt/main.cpp` after
  `nsl::dialect::registerNSLDialect()` and before `MlirOptMain`.

---

## 3. Internal-only symbols (MUST NOT be re-exported)

The following are explicitly NOT part of the M5 public surface:

- The `ASTToMLIR` class itself (declared in `lib/Lower/ASTToMLIR.h`).
  Consumers use the `astToMLIR(...)` free function.
- The six `NSL<X>Pass` classes (declared in
  `lib/Lower/Pass/<Name>.cpp`'s anonymous namespace or local header).
  Consumers use the `create<X>Pass()` free functions.
- The `DiagnosticBridge` class (private under `lib/Lower/Pass/Common/`).
- Any per-pass helper (struct-SSA-split utilities, generate-unroll
  internals, etc.).

If a future tooling requirement (e.g., LSP `nsl-lsp` direct visitor
invocation) needs access to one of these, that requirement justifies
a contract amendment expanding the §2 freeze list — not silent
re-export.

---

## 4. Header dependencies (re-exports inside `Lower.h`)

`Lower.h` MAY include the following upstream headers (consumer
sees them transitively):

- `<mlir/IR/MLIRContext.h>` — required for `MLIRContext&` parameter
- `<mlir/IR/BuiltinOps.h>` — required for `ModuleOp` return type
- `<mlir/IR/OwningOpRef.h>` — required for `OwningOpRef<ModuleOp>` return
- `<mlir/Pass/Pass.h>` — required for `mlir::Pass`
- `<memory>` — required for `std::unique_ptr`
- `nsl/AST/CompilationUnit.h` — required for `ast::CompilationUnit&` parameter
- `nsl/Sema/Sema.h` — required for `sema::SemaResult&` parameter

`Lower.h` MUST NOT include `nsl/Dialect/NSL/IR/NSLDialect.h` —
the visitor's IR-construction details are a private implementation
concern, not a public surface. (Consumers wanting to query
`nsl::*` ops on the returned `ModuleOp` include `NSLDialect.h`
themselves; that's an `nsl-dialect` consumer-decision, not an
`nsl-lower` re-export.)

---

## 5. CMake link-dependency contract

`add_nsl_library(nsl-lower …)` declaration in
`lib/Lower/CMakeLists.txt` is M0-frozen as:

```cmake
add_nsl_library(nsl-lower
  DEPENDS nsl-sema nsl-dialect
  LINK_LIBS
    CIRCTHW
    CIRCTComb
    CIRCTSeq
    CIRCTSV)
```

M5 amendments to this declaration are limited to:

- **Source-list growth**: appending the new `.cpp` files (visitor +
  six passes + `DiagnosticBridge`) to `add_nsl_library`'s implicit
  source list (the macro expands `${CMAKE_CURRENT_SOURCE_DIR}/*.cpp`).
- **No** changes to `DEPENDS` (M5 needs `nsl-basic` transitively
  via `nsl-sema`; explicit listing is unnecessary).
- **No** changes to `LINK_LIBS` (CIRCT libs stay inert at M5).

Any other CMake amendment is a contract change (e.g., adding
`nsl-ast` to `DEPENDS` would be redundant — `nsl-sema` transitively
provides AST types — but if a future PR removes that transitivity,
this contract is amended in the same change).

---

## 6. ABI surface count

**Total exported symbols at M5 freeze**: **8 free functions + 0 classes + 0 types**.

- 1 visitor entry point (`astToMLIR`)
- 6 `create<X>Pass` constructors
- 1 registration helper (`registerNSLLowerPasses`)

This count is the analogue of M4's `dialect-api.contract.md` §2
freeze count (48 public types/functions). A future PR adding any
public symbol to `Lower.h` MUST update this number AND justify the
addition against Constitution Principle II's single-public-header
rule.

### 6.1 US1 ship status

**US1 (`nslc -emit=mlir` produces verified `nsl.*` IR for every AST
shape) ships on branch `008-m5-structural-passes` with the
T057+T058+T059 close-out commit** — see
[`tasks.md`](../tasks.md) for the task scorecard and
`git log --grep="US1 ships clean"` for the close-out commit hash.
Final scorecard at close-out:

- 471/471 `check-nslc` lit tests PASS.
- 32/32 `scripts/m5_smoke.sh` round-trip-clean fixtures PASS.
- `scripts/audit_lower_fixtures.sh` exits 0: 37 concrete `visit()`
  overrides covered (29 via fixture, 8 via allow-list); 17 STUB
  no-op slots ignored; 32 fixtures inventoried under `test/Lower/`.
- Audit hooked into `scripts/ci.sh static-checks` step 5
  (CI-blocking on missing-fixture per FR-027 + Principle IX).

US2/US3/US4 (the structural-expansion passes) and US5 (the
determinism CI gate) remain. Their pass-bodies are still NO-OP
slots from Phase 2 (T007–T012); the headline `-emit=mlir` pipeline
runs them as no-ops, which is sound because every M3-Sema-clean
fixture in scope for US1 has zero `generate` / `variable` /
`%IDENT%` / `param_int` / array-submod content.
