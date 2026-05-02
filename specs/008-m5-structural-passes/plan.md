<!-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception -->

# Implementation Plan: M5 — `nsl-lower` part 1 (AST → `nsl` dialect + structural-expansion passes)

**Branch**: `008-m5-structural-passes` | **Date**: 2026-04-30 | **Spec**: [spec.md](./spec.md)
**Input**: Feature specification from `/specs/008-m5-structural-passes/spec.md`

## Summary

M5 delivers the first half of the `nsl-lower` library (layer 8a):
the AST → `nsl` MLIR dialect lowering visitor (`ASTToMLIR`) plus
the six-pass structural-expansion pipeline that rewrites `nsl::*`
IR in-place before any CIRCT lowering. The user-visible deliverable
is a working `nslc -emit=mlir input.nsl` driver flag whose output
is byte-stable across builds and round-trippable via the M4
`nsl-opt` binary. Headline sub-deliverables per README's M5 row
parenthetical: generate-loop unroll (`NSLExpandGeneratePass`),
struct-SSA-split (`NSLExpandVariablesPass`), and `%IDENT%`
residue check (`NSLCheckSemanticsPass`). Three more passes round
out the design §9 list (`NSLResolveParamsPass`,
`NSLExplodeSubmodArrayPass`, `NSLInlineInternalFuncPass` — the
last as a no-op slot at M5 per Q3 → Option B).

The technical approach (see [research.md](./research.md) for
Decision/Rationale/Alternatives per choice): single-pass AST
walker with MLIR's stock `SymbolTable` lazy resolution (Q4 →
Option A); regex-based residue detection over `mlir::StringAttr`
values (Q1 → Option B); MLIR default printer for `-emit=mlir`
(Q2 → Option A); shared `DiagnosticBridge` (RAII handler)
forwarding all MLIR diagnostics to `basic::DiagnosticEngine`
(FR-019); CI grep enforcing absence of pointer-derived ordering
and host-path leakage (research §13).

## Technical Context

**Language/Version**: C++17 across `nsl-lower` (Constitution
"Build, Code, and Licensing Standards"). C++20 features prohibited.

**Primary Dependencies**: LLVM 18 + MLIR 18 + CIRCT (matched to
the `ghcr.io/koyamanx/nsl-nslc:dev` container's pinned versions —
M0 contract). M5 uses MLIR's `IR`, `Pass`, and `Support` libraries
plus `mlir::ScopedDiagnosticHandler`; CIRCT link-libs declared in
`lib/Lower/CMakeLists.txt` stay inert at M5 (M6 activates them).

**Storage**: N/A (compiler frontend; no persistent state).

**Testing**: lit + FileCheck (per Constitution Principle VI per-
layer accepted test driver: "Lowering tests use lit + FileCheck").
Per-AST-node fixtures under `test/Lower/<category>/`; per-pass
fixtures under `test/Lower/passes/<pass-flag>/`; M3-corpus
extension under `test/Lower/m3_corpus/`; determinism gate runs in
CI matrix (no per-test goldens, just `diff -q` across two host
paths). gtest only for unit-level helpers (e.g.,
`DiagnosticBridge` translation table) — the bulk of M5 testing is
lit-driven.

**Target Platform**: Linux x86_64 (Constitution Principle IX
build matrix); other platforms forward-looking. Dev container is
canonical (`ghcr.io/koyamanx/nsl-nslc:dev`).

**Project Type**: Compiler library + driver (single project,
LLVM-style layered architecture per Constitution Principle II).

**Performance Goals**: M5 lit corpus completes in under 30 s in
CI (Principle IX stage 4 timing budget); largest single-fixture
pass-pipeline runtime under 100 ms wall-clock (informational, not
a contract surface — see `pass-pipeline.contract.md` §7).

**Constraints**: Determinism (Principle V) — every code path
producing a name MUST use stable iteration; CI grep enforces no
`std::unordered_*` / pointer-derived ordering / time sources in
`lib/Lower/`. Single public umbrella header (Principle II) —
`nsl-lower` is NOT one of the named exceptions for `nsl-ast` /
`nsl-sema`. Diagnostic plumbing (Principle IV) — every emitted op
carries `mlir::Location`; every pass diagnostic flows through the
project `basic::DiagnosticEngine`.

**Scale/Scope**: 8 public symbols exported from `Lower.h` (count
frozen by `lower-api.contract.md` §6); ~30 `visit()` overrides on
`ASTToMLIR`; six pass implementations averaging ~150–300 LOC each
(`NSLExpandVariablesPass` is heaviest); ~40–50 per-AST-node
fixtures + ~25 per-pass fixtures + ~34 M3-corpus extension
fixtures = ~100–110 lit fixtures total at M5 acceptance.

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-check after Phase 1
design.*

### Phase 0 (pre-research) gate

| Principle | Status | Notes |
|---|---|---|
| I. Spec Is Authoritative | **Pass** | M5 introduces no new `Sn`/`Nn`/`Pn`. The `%IDENT%` residue check is the operationalisation of an existing P3 invariant + Principle IV. The "no silent AST drops" sub-clause does not apply at this layer (M5 consumes the M2/M3-built AST; it does not parse). |
| II. Layered Library Architecture | **Pass** | `nsl-lower` is layer 8a; depends on `nsl-sema` + `nsl-dialect` only (Principle II § layer table). Single public umbrella header `Lower.h` (NOT a named exception). `nsl-opt` reuses `libNSLFrontend.a` via the standard link path. |
| III. Stock CIRCT Below | **Pass** | Zero `circt::*` references at M5; CIRCT link-libs in `lib/Lower/CMakeLists.txt` stay inert. The seam between `nsl::*` dialect (project surface) and CIRCT (stock infrastructure below) is preserved. |
| IV. Source-Locating Diagnostics | **Pass** | FR-008 mandates non-`UnknownLoc` on every emitted op; FR-019 routes every pass diagnostic through `basic::DiagnosticEngine`; SC-009 asserts via CI walk. |
| V. Inspectable, Deterministic Pipeline | **Pass** | New `-emit=mlir` flag (FR-022) + determinism contract (FR-025/FR-026, US5, SC-007/SC-008). Research §13 lays out the audit-by-CI-grep rule. |
| VI. Layered Test Discipline | **Pass** | Lowering tests use lit + FileCheck per the per-layer accepted-driver clause; M3-corpus extension extends Sema's coverage to the new layer (FR-030). Audited corpus is M7, NOT M5. |
| VII. Spec ↔ Design Coupling | **Pass** | FR-031 lists the cross-doc updates (`CLAUDE.md` §1, `docs/CLAUDE.md` §3). No `Sn`/`Nn`/`Pn` change → no quick-map / line-range update needed. |
| VIII. Test-First Development | **Pass** | Quickstart §3 prescribes the first failing fixture; T-order in §4 follows TDD (test → implement → green). FR-018's diagnostic-string is frozen by `residue-detection.contract.md` §4 for the Principle VIII string-stability rule. |
| IX. Continuous Integration & Delivery | **Pass** | All six CI stages exercise M5 deliverables: build (stage 1), static checks (stage 2 — including the new `audit_determinism.sh`), unit + layer (stage 3 — new `check-nsl-lower` ninja target), lowering (stage 4 — the M5 lit corpus), end-to-end (stage 5 — vacuous at M5; lights up at M7), formal (stage 6 — vacuous at M5; lights up at M8). |

**Gate result**: ✅ All nine principles pass at the pre-research
gate. No transitional clauses invoked.

### Phase 1 (post-design) re-check

After authoring research.md / data-model.md / contracts/ /
quickstart.md, the gate is re-evaluated. Findings:

| Principle | Re-evaluation result |
|---|---|
| II. Layered Library Architecture | **Confirmed** — `lower-api.contract.md` §6 freezes the 8-symbol public surface; `research.md` §6 confirms internal-header layout under `lib/Lower/Pass/` is private. No silent additions. |
| III. Stock CIRCT Below | **Confirmed** — no contract / data-model entry references `circt::*` symbols. The Pass classes derive only from `mlir::OperationPass<nsl::dialect::ModuleOp>`. |
| IV. Source-Locating Diagnostics | **Confirmed** — `data-model.md` §1 invariant + `pass-pipeline.contract.md` §6 forwarding rules + `residue-detection.contract.md` §4 location-translation lock the diagnostic-source-locating chain end to end. |
| V. Inspectable, Deterministic Pipeline | **Confirmed** — `driver-emit-mlir.contract.md` §3 freezes the determinism axes (4-way diff matrix); research §13 freezes the audit-grep regex; `data-model.md` §1 prohibits pointer-iteration. |
| VI. Layered Test Discipline | **Confirmed** — `data-model.md` §7 catalogs the four fixture-corpus shapes; FR-027 / FR-028 / FR-029 / FR-030 each have a §7 sub-section. |
| VIII. Test-First Development | **Confirmed** — `residue-detection.contract.md` §8 freezes the diagnostic-string per fixture; quickstart §3 prescribes the failing-first commit. The constructive carve-out does not apply here (no new `Sn`). |
| IX. Continuous Integration & Delivery | **Confirmed** — quickstart §10 enumerates the per-stage CI gates; no bypass paths surface in any contract. |

**Phase 1 gate result**: ✅ No new violations introduced by
design. Complexity Tracking section below is empty (no
exceptional carve-outs needed).

## Project Structure

### Documentation (this feature)

```text
specs/008-m5-structural-passes/
├── plan.md                                  # This file
├── research.md                              # Phase 0 — Decision/Rationale/Alternatives × 14
├── data-model.md                            # Phase 1 — entity catalog
├── quickstart.md                            # Phase 1 — developer onboarding
├── contracts/
│   ├── lower-api.contract.md                # 8-symbol public surface freeze
│   ├── pass-pipeline.contract.md            # Six-pass ordering + post-conditions
│   ├── driver-emit-mlir.contract.md         # CLI flag + default-printer freeze
│   └── residue-detection.contract.md        # Regex + scanned attr-table freeze
├── checklists/
│   └── requirements.md                      # Spec quality checklist (from /speckit-specify)
├── spec.md                                  # The feature specification
└── tasks.md                                 # Phase 2 — /speckit-tasks output (NOT created here)
```

### Source Code (repository root)

The M5 deliverable touches the following library trees:

```text
include/nsl/Lower/
└── Lower.h                                  # NEW: public umbrella (8 symbols frozen)

lib/Lower/
├── ASTToMLIR.h                              # NEW: visitor class declaration (private)
├── ASTToMLIR.cpp                            # NEW: visitor implementation
├── Pass/
│   ├── NSLResolveParamsPass.cpp             # NEW: slot 1
│   ├── NSLExpandGeneratePass.cpp            # NEW: slot 2 (headline)
│   ├── NSLExpandVariablesPass.cpp           # NEW: slot 3 (headline)
│   ├── NSLExplodeSubmodArrayPass.cpp        # NEW: slot 4
│   ├── NSLInlineInternalFuncPass.cpp        # NEW: slot 5 (no-op at M5)
│   └── NSLCheckSemanticsPass.cpp            # NEW: slot 6 (residue + sensitive-Sn)
├── Pass/Common/
│   ├── DiagnosticBridge.h                   # NEW: ScopedDiagnosticHandler RAII wrapper
│   └── DiagnosticBridge.cpp
└── CMakeLists.txt                           # AMEND: extend source-list (no DEPENDS / LINK_LIBS change)

lib/Driver/
├── Compilation.cpp                          # AMEND: wire EmitKind::NSLMLIR arm
├── LowerToNSL.cpp                           # AMEND: M4 stub → real body
├── RunNSLPasses.cpp                         # AMEND: M4 stub → real body
└── CMakeLists.txt                           # AMEND: add nsl-lower link dependency

tools/nsl-opt/
├── main.cpp                                 # AMEND: one new line — registerNSLLowerPasses()
└── CMakeLists.txt                           # AMEND: add nsl-lower link dependency

tools/nslc/
└── main.cpp                                 # UNCHANGED (M2-frozen ~60-line entry; delegates to nsl-driver)

test/Lower/
├── decl/                                    # NEW: per-AST-node fixtures (FR-027)
├── module/
├── action/
├── stmt/
├── expr/
├── marker/
├── passes/                                  # NEW: per-pass fixtures (FR-028)
│   ├── nsl-resolve-params/
│   ├── nsl-expand-generate/
│   ├── nsl-expand-variables/
│   ├── nsl-explode-submod-array/
│   ├── nsl-inline-internal-func/
│   └── nsl-check-semantics/
├── m3_corpus/                               # NEW: M3-corpus extension (FR-030)
│   └── s<NN>/<case>.expected.mlir
├── determinism/                             # NEW: cross-build matrix gate (FR-029)
└── lit.cfg.py                               # AMEND: register the new test directories

scripts/
├── audit_determinism.sh                     # NEW: CI grep for pointer/time leaks (research §13)
├── audit_lower_fixtures.sh                  # NEW: CI grep enforcing FR-027 enumeration
└── ci.sh                                    # AMEND: invoke the two new audit scripts in stage 2

CLAUDE.md                                    # AMEND: SPECKIT marker block + §1 confirmations
docs/CLAUDE.md                               # AMEND: §3 (line ranges if §9 shifted)
```

**Structure Decision**: Single project (Option 1 from the plan
template), LLVM-style. Layered library architecture per
Constitution Principle II — no `frontend/` / `backend/` split (the
compiler IS the deliverable). M5 fits cleanly into the existing
nine-library tree by extending `lib/Lower/` (which has been a
M0-stub since the project bootstrap).

## Complexity Tracking

> **Fill ONLY if Constitution Check has violations that must be justified**

Empty — no Constitution violations surfaced at either gate. All
nine principles pass cleanly with the four Clarifications-resolved
choices and the design described above.

## Phase Cross-References

- **Phase 0 (research)**: [`research.md`](./research.md) §§1–14 —
  fourteen Decision/Rationale/Alternatives entries covering all
  four Clarifications (Q1–Q4) plus ten "deferred to plan" items
  surfaced during spec authoring.
- **Phase 1 (data model)**: [`data-model.md`](./data-model.md) — nine
  entity sections (visitor, six passes, residue, Compilation
  driver, EmitKind, DiagnosticBridge, fixture taxonomy, upstream
  relationships, IR-shape state transitions).
- **Phase 1 (contracts)**: [`contracts/`](./contracts/) — four
  `.contract.md` files freezing the public surfaces (8-symbol
  library API, six-pass pipeline ordering, `-emit=mlir` driver
  behaviour, residue-detection mechanism).
- **Phase 1 (quickstart)**: [`quickstart.md`](./quickstart.md) —
  developer onboarding with the suggested T-1..T-25
  implementation order, build/test loop, determinism-verification
  recipe, and pre-merge checklist.
- **Phase 2 (tasks)**: NOT generated by this command — run
  `/speckit-tasks` next.
