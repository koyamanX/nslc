<!-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception -->

# Contract: M5 Pass-Pipeline Behaviour

**Branch**: `008-m5-structural-passes` | **Date**: 2026-04-30
**Spec**: [spec.md](../spec.md) FR-011 … FR-019, FR-021

This contract freezes the six-pass pipeline assembled inside
`Compilation::runNSLPasses`: pass identity, pass ordering, pass
post-conditions, and the pipeline's failure-propagation semantics.
Any change after M5 freeze is a contract amendment and MUST update
this file in the same patch (Principle VII).

---

## 1. Pipeline structure (frozen by FR-012)

```text
Compilation::runNSLPasses(mlir::ModuleOp module):
    PassManager pm(ctx, ModuleOp::getOperationName());

    pm.addPass(createNSLResolveParamsPass());        // slot 1
    pm.addPass(createNSLExpandGeneratePass());       // slot 2
    pm.addPass(createNSLExpandVariablesPass());      // slot 3
    pm.addPass(createNSLExplodeSubmodArrayPass());   // slot 4
    pm.addPass(createNSLInlineInternalFuncPass());   // slot 5 (no-op at M5)
    pm.addPass(createNSLCheckSemanticsPass());       // slot 6

    return pm.run(module);
```

**Reordering forbidden**: any departure from the slot-1..slot-6
sequence is a contract amendment. Rationale per FR-012:

- Slot 1 must precede slot 2: param resolution produces the
  constants slot 2 consumes for `generate` bound expressions.
- Slot 2 must precede slot 3: `generate` unrolling replicates
  bodies; variable expansion inside replicated bodies needs
  consistent SSA value-version naming, which only works after
  unrolling stops creating new copies.
- Slot 6 must be last: residue + sensitive-`Sn` re-check requires
  the post-expansion shape.

---

## 2. Per-pass freeze table

| # | Slot | CLI flag | Class | Pre-condition | Post-condition | Diagnostic surface |
|---|---|---|---|---|---|---|
| 1 | `NSLResolveParamsPass` | `-nsl-resolve-params` | `nsl::lower::NSLResolveParamsPass` | input may contain `nsl.param_int` / `nsl.param_str` operand refs anywhere | zero unresolved param refs in the entire `ModuleOp` | none on success; `error: unresolved param '@<name>' (no entry in Sema parameter map)` per missing param |
| 2 | `NSLExpandGeneratePass` | `-nsl-expand-generate` | `nsl::lower::NSLExpandGeneratePass` | input may contain `nsl.structural_generate` ops; loop bounds are integer-typed (S10 enforced upstream) | zero `nsl.structural_generate`; loop-var `%IDENT%` references substituted with per-iteration constants; replicated bodies carry `unroll_index` attribute (research §6 not specified — the attr is implementation-internal not contract-frozen) | none on success; `error: generate bound is non-constant after param resolution` per non-constant bound (cannot happen if slot 1 ran cleanly) |
| 3 | `NSLExpandVariablesPass` | `-nsl-expand-variables` | `nsl::lower::NSLExpandVariablesPass` | input may contain `nsl.variable` ops; struct-typed variables OK | zero `nsl.variable`; SSA chain of `nsl.wire`+`nsl.transfer` per variable; per-field decomposition for struct-typed; `S12` partial-assignment patterns preserved | none on success |
| 4 | `NSLExplodeSubmodArrayPass` | `-nsl-explode-submod-array` | `nsl::lower::NSLExplodeSubmodArrayPass` | input may contain `nsl.submodule` with array-form names | zero array-form `nsl.submodule`; per-element instances `SUB[0]`..`SUB[N-1]`; cross-IR port-references rewritten | none on success |
| 5 | `NSLInlineInternalFuncPass` | `-nsl-inline-internal-func` | `nsl::lower::NSLInlineInternalFuncPass` | any input | IR byte-identical to input (M5 no-op slot per Q3 → Option B) | none |
| 6 | `NSLCheckSemanticsPass` | `-nsl-check-semantics` | `nsl::lower::NSLCheckSemanticsPass` | input is post-expansion shape | unchanged on success; one diagnostic per residue match + one per sensitive-`Sn` violation otherwise | `error: unresolved macro splice '%<IDENT>%' after structural expansion`; per-`Sn` diagnostic strings (see §3 below) |

---

## 3. `NSLCheckSemanticsPass` re-check subset (frozen by research §5)

The pass re-checks **exactly six** post-expansion-sensitive `Sn`
constraints. The diagnostic-string contract for each is frozen at
M5 (renaming requires a contract amendment per Principle VIII's
diagnostic-message-string-frozen rule):

| `Sn` | Trigger condition | Diagnostic string |
|---|---|---|
| `S6` | post-expand-variables wire-chain has out-of-order use → def | `error: register or wire '<name>' used before definition` |
| `S10` | loop variable still present (slot 2 cleanup failed) | `error: 'generate' loop variable '%<name>%' not eliminated by structural expansion` |
| `S15` | bit-slice index resolves to non-compile-time-constant after slot 1 | `error: bit-slice index is non-constant after parameter resolution` |
| `S16` | `param_int` / `param_str` ref survives in pure-NSL module | `error: parameter '@<name>' meaningful only for V/V/SC submodules` |
| `S20` | submod-array element lacks parent's interface modifier binding | `error: submodule '<name>[<i>]' missing interface modifier from parent` |
| `S25` | replicated-body emits two decls with the same name | `error: duplicate declaration '<name>' in replicated 'generate' body` |

The 23 other `Sn` constraints are **NOT** re-checked by
`NSLCheckSemanticsPass` per research §5. Adding any of them post-M5
is a contract amendment. Constructive-Sn (`S13`/`S18`/`S19`/`S23`/`S24`/`S27`)
are **NEVER** re-checked here — by definition they emit no
diagnostic; their introspection observable lives at the M3 Sema
layer (per Constitution v1.6.0 Principle VIII constructive
carve-out).

---

## 4. Failure-propagation semantics

**Per-pass failure**:

- A pass calls `signalPassFailure()` → `runOnOperation()` returns
  whatever, but `mlir::PassManager::run()` returns
  `mlir::failure()`.
- The pipeline halts at the failing slot — slots `i+1`..`6` do NOT
  execute (matches MLIR's default `PassManager` behaviour).
- The failing pass MUST have already emitted at least one
  diagnostic via `op->emitError(...)` / `op->emitOpError(...)`
  before signalling failure.

**Multi-error within one pass**:

- `NSLCheckSemanticsPass` (slot 6) MUST emit ALL diagnostics for
  ALL violations in a single `runOnOperation()` invocation
  (FR-018). It does NOT short-circuit after the first violation.
- After emitting all diagnostics, the pass calls
  `signalPassFailure()` IF AND ONLY IF the diagnostic count > 0.
- This matches the M2 parser / M3 Sema multi-error-recovery
  convention.

**`Compilation::runNSLPasses` return**:

- `mlir::success()` ↔ all six passes ran cleanly.
- `mlir::failure()` ↔ at least one pass signalled failure
  (diagnostics already on the engine).

**Driver consequence**:

```cpp
if (mlir::failed(runNSLPasses(*module))) {
    return printDiagsAndExitNonZero();  // no .mlir output produced
}
```

---

## 5. `nsl-opt` standalone-pass invocation

Each of the six passes MUST be invocable from `nsl-opt` standalone:

```text
nsl-opt -nsl-resolve-params input.mlir > output.mlir
nsl-opt -nsl-expand-generate input.mlir > output.mlir
nsl-opt -nsl-expand-variables input.mlir > output.mlir
nsl-opt -nsl-explode-submod-array input.mlir > output.mlir
nsl-opt -nsl-inline-internal-func input.mlir > output.mlir   # no-op at M5
nsl-opt -nsl-check-semantics input.mlir > output.mlir        # may exit non-zero
```

Pass chaining via `nsl-opt -pass-pipeline=...` is supported
(MLIR-built-in mechanism). The pipeline string for the full M5
pipeline is:

```text
builtin.module(nsl-resolve-params,nsl-expand-generate,nsl-expand-variables,nsl-explode-submod-array,nsl-inline-internal-func,nsl-check-semantics)
```

This invocation is byte-equivalent to `nslc -emit=mlir input.nsl`
(modulo the `nsl-opt` taking pre-built `.mlir` instead of `.nsl`
source).

`nsl-opt --help` MUST list all six passes with the descriptions
from `getDescription()` per SC-002.

---

## 6. Diagnostic forwarding contract

Per FR-019 and research §12:

- Every pass emits diagnostics ONLY through MLIR's
  `op->emitError(...)` / `op->emitOpError(...)` /
  `op->emitWarning(...)` / `op->emitRemark(...)` / `op->emitNote(...)`.
- The active `DiagnosticBridge` (constructed at the top of
  `Compilation::runNSLPasses`) intercepts every diagnostic and
  posts an equivalent `basic::Diagnostic` to the project's
  `basic::DiagnosticEngine`.
- The `DiagnosticBridge` translation table is frozen by
  `data-model.md` §6 (Note→Note, Warning→Warning, Error→Error,
  Remark→Note).
- `mlir::Location` → `basic::SourceRange` translation prefers
  `FileLineColLoc`, falls back to `FusedLoc` deepest-first walk.
- No pass uses `llvm::errs()` / `std::cerr` directly. CI grep
  (per research §13) catches violations.

---

## 7. Performance contract (informational, NOT a freeze surface)

The pipeline is expected to run in **under 100 ms wall-clock** on
the largest M3-corpus fixture (cardinality ≤200 ops). This is not
a frozen SC item — it is a sanity-check budget. If a fixture
exceeds 1 second, the implementer SHOULD investigate (likely
indicates accidental quadratic behaviour in a pass).

The full M5 lit + ctest matrix is expected to complete in **under
30 seconds** on the dev container per Principle IX stage 4
(lowering tests). The audited-corpus end-to-end gate is M7's
budget, not M5's.
