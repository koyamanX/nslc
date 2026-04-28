<!-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception -->

# Implementation Plan: M3 — Sema (`nsl-sema`: SymbolTable + TypeSystem + S1–S29)

**Branch**: `006-m3-sema` | **Date**: 2026-04-28 | **Spec**: [spec.md](./spec.md)
**Input**: Feature specification from `/specs/006-m3-sema/spec.md`

## Summary

Land the next compiler-track library — `nsl-sema` (6) — and the
driver glue that runs Sema after parse for every `-emit=*` from
`-emit=ast` forward. M2 delivered the parser + AST; M3 turns the
parser-emitted `CompilationUnit` into a *resolved* AST: every
identifier reference points to a `Symbol*`, every `Expr` carries a
`TypeRef inferredType()` filled by width inference, and every
violation of a semantic constraint `S1`–`S29` produces a precise,
source-locating diagnostic. **M3 is the unlock point** — every
later milestone (M4 dialect, M5 lowering, M6 CIRCT, M7 end-to-end,
M8 formal, M9 release) consumes the resolved AST, and the
tooling-track gates that read the symbol table directly (T2
formatter, T3 LSP skeleton, T6 lint framework) all open here.

Deliverables, all mandated by the spec (FR cross-references in
parens):

- **`nsl-sema`** populates `include/nsl/Sema/` with three umbrella
  headers — `Sema.h` (entry-point + `SemaResult`),
  `SymbolTable.h` (`Symbol` hierarchy + `Scope` stack), and
  `TypeSystem.h` (`Type` hierarchy + `TypeRef` interner). Concrete
  `Symbol` subclasses mirror
  [`docs/design/nsl_compiler_design.md`](../../docs/design/nsl_compiler_design.md)
  §6 (lines 688–795) verbatim — `PortSymbol`, `RegSymbol`,
  `WireSymbol`, …, `StructTypeSymbol`. Concrete `Type` subclasses
  mirror §6.x (lines 797–856): `Bit`, `BitVector(N)`,
  `Struct(name)`, `Memory(depth, element)`, `Unresolved`.
  FR-001, FR-004, FR-005, FR-007.
- **Sema engine** (`lib/Sema/Sema.cpp`) implements the **hybrid
  pass strategy** resolved in Clarifications session 2026-04-28
  Q3 → Option C: one top-down `ResolutionPass` walks the AST once
  populating the `SymbolTable` and emitting exactly one diagnostic
  per unresolved name (no cascade per FR-017); then a per-`Sn`
  `ConstraintCheckPass` set runs each constraint over the full AST
  as an independent walker. The width-inference top-down pass
  (per design §6.x line 856) is folded into `ResolutionPass` so
  the per-`Sn` checks consume already-typed `Expr` nodes. FR-008,
  FR-016, FR-017.
- **Per-`Sn` constraint checkers** under `lib/Sema/Constraints/`:
  one source file per constraint family, each registering one
  visitor with the per-`Sn` driver. The 23 *error/warning*
  constraints attach a frozen diagnostic-message string per FR-011
  / FR-015; the 6 *constructive* constraints (`S13`, `S18`, `S19`,
  `S23`, `S24`, `S27`, per FR-013 and Clarifications Q1 → Option B)
  expose a tested introspection surface instead. FR-010 through
  FR-013.
- **Mechanical fix-it hints** for `S3` (`=` vs `:=` on a reg),
  `S7` (`seq` outside func/proc), `S14` (missing conditional-expr
  `else`), per FR-012; each fix-it carries a `SourceRange` +
  `replacement` and is asserted alongside the message text in the
  fail fixture.
- **`nslc` driver glue**: `Compilation::sema()` runs after
  `parse()` for every `-emit=*` from `-emit=ast` forward (per
  FR-019). On Sema failure the driver exits non-zero and emits no
  later-stage output. The `-emit=ast` printer is taught to detect
  post-Sema input and emit the additive `: <type>` and `→ decl@<file>:<line>:<col>`
  suffixes per Clarifications Q2 → Option A (FR-020). The M2
  parser-only `-emit=ast` golden corpus is re-cut in this same
  patch (Principle VII).
- **Test corpus** under `test/sema/` — Principle VI's NON-NEGOTIABLE
  pass+fail-per-`Sn` discipline applied: 29 directories
  `s01/`…`s29/`, each with `pass.nsl` and `fail.nsl`, the latter
  asserting the literal diagnostic text (for the 23 error/warning
  rows) or the flipped introspection-expected-value (for the 6
  constructive rows). Plus `test/sema/recovery/` (multi-error
  fixture corpus per FR-025), `test/sema/resolution/` (per-scope
  + per-symbol-kind name resolution per FR-026), and
  `test/sema/width/` (per-`Expr`-form width inference per FR-027).
  FR-023 through FR-028.

Three /speckit-clarify decisions (session 2026-04-28) frame the
scope: **Q1** — paired pass + introspection for the 6 constructive
`Sn` (no diagnostic-string assertion for those, since they emit no
diagnostic); **Q2** — `-emit=ast` re-cut in place at M3 (one flag,
format drifts additively); **Q3** — hybrid recovery (one resolution
pass, then per-`Sn` independent passes). M3's implementation latitude
is in *how* the resolver and the per-`Sn` walkers are coded, not
*what* they accept or reject.

## Technical Context

**Language/Version**: **C++17** across the library and the driver
glue (Constitution Build/Code/Licensing — C++20 forbidden until
amendment). The Sema engine is hand-written single-pass resolution
+ N-pass per-`Sn` walkers (no analysis-framework dependency); the
visitor uses `nsl-ast`'s `ASTVisitor` polymorphic base. Helper
scripts in **Python 3.8+** matching M0/M1/M2 baseline.
**Primary Dependencies**: **LLVM** at the CIRCT-pinned commit
(vendored prebuilt; M0 §2). M3 uses LLVM only for `llvm::DenseMap`
(symbol-table buckets), `llvm::StringRef` / `llvm::ArrayRef` /
`llvm::SmallVector` (per design §6.x lines 825, 847–851), and the
bundled **GoogleTest** + **lit + FileCheck** drivers. **No new
external dependencies introduced at M3.** M2's `nsl-ast` is
consumed via its public per-node-kind headers; M1's `nsl-basic`
(`SourceManager`, `DiagnosticEngine`, `SourceRange`) is consumed
via its public headers exclusively.
**Storage**: N/A. The `SymbolTable`, `TypeSystem`, and resolved-AST
state are in-memory only; ownership transfers from `Compilation`
to `SemaResult` so later stages can walk the post-Sema AST without
re-running resolution. No persistent symbol-store at M3 (the
incremental-resolution cache is T-track LSP infrastructure, not
M-track).
**Testing**: **GoogleTest** (`test_unit/`) for unit-level fixtures
of the `SymbolTable` scope-stack invariants, the `TypeSystem`
interning equality contract, the `ResolutionPass` no-cascade
guarantee (FR-017), and the paired-introspection assertions for
the 6 constructive `Sn` (per Clarifications Q1 → Option B).
**lit + FileCheck** (`test/sema/`) for the per-`Sn` pass + fail
fixture pairs, the multi-error recovery corpus, the per-scope
resolution corpus, the per-`Expr`-form width corpus, the post-Sema
`-emit=ast` golden, and the diagnostic-string assertions for the 23
error/warning `Sn`. Per Constitution Principle VI ("lit + FileCheck
— no substitutes" for lowering tests; M3 reuses the convention for
diagnostic-bearing fixtures because the artifact under test is the
diagnostic text itself). Per-fixture pass+fail discipline (Principle
VIII) for every `Sn` with a violation case; one test per `S1`–`S29`
per Principle VI's "Sema tests" bullet (line 243).
**Target Platform**: **Linux x86_64** (M0/M1/M2 baseline). Other
architectures and operating systems remain deferred.
**Project Type**: Compiler frontend — extends M2's five-layer
front-end with a sixth (`nsl-sema`) and the driver-glue stage that
runs Sema between parse and any later `-emit=*`. No new CLI flags
at M3 (per Q2 Option A's "re-cut in place" decision; FR-021).
**Performance Goals**: `nslc -emit=ast` (post-Sema) finishes in
**< 2 s** on a representative single-file input on the reference
host (informal; ~2× the M2 budget to absorb the resolution pass
+ ~29 per-`Sn` walks; no per-line throughput SLO at M3 — deferred
to M7's audited-corpus regression where it has a measurement
basis). Per-`Sn` walker overhead is expected to be modest — each
walker visits only the AST node kinds relevant to its `Sn` (e.g.,
`S2` walks `WireDecl`-with-init only); the worst case is the full
~29 walks compounded with one resolution pass, all linear in AST
size.
**Constraints**: **Byte-stable post-Sema AST output** (FR-029,
Principle V) — two `nslc -emit=ast` invocations on the same input
+ flag list MUST produce byte-identical stdout, including the new
`: <type>` and `→ decl@…` enrichments (Q2 Option A). **Symbol
identity not pointer-leaked** (FR-031) — cross-references serialize
via the target's `declLoc.start` byte offset or a zero-based
monotonic symbol-table index. **Deterministic collection iteration**
(FR-030) — the `SymbolTable` MUST iterate scopes in
insertion-order; `TypeSystem` interner uses `llvm::DenseMap` for
lookup but `llvm::SmallVector` for the printer's iteration order.
**No-cascade guarantee** (FR-017) — exactly one diagnostic per
unresolved name, period; downstream `Sn` checks gate on
`Unresolved`-typed subtrees from the resolution pass and skip
them silently. **Layered structure preserved** (FR-002, FR-003) —
`nsl-sema` MUST NOT depend on `nsl-parse`, `nsl-dialect`,
`nsl-lower`, or `nsl-driver`; the same `scripts/check_layering.py`
that M2 introduced (extended at M3 to forbid `nsl-sema`→`nsl-parse`
edges) verifies on every build.
**Scale/Scope**: 1 library (~6 .cpp + ~3 .h files in `lib/Sema/` +
`include/nsl/Sema/`, plus ~13 per-symbol-kind sub-classes folded
into `SymbolTable.h` and ~5 per-type-kind sub-classes folded into
`TypeSystem.h`); 1 driver-glue source (`lib/Driver/Sema.cpp` +
modifications to `lib/Driver/EmitAST.cpp` to call Sema). Fixture
count: **58 per-`Sn` fixtures** (29 × pass + fail), **~6 multi-error
recovery fixtures** (FR-025 corpus minimum + a few extras), **~10
per-scope resolution fixtures** (one per scope kind × per scoped-
name form, FR-026), **~12 per-`Expr`-form width fixtures**
(one per `Expr` kind whose width is Sema-determined, FR-027), 1
re-cut `-emit=ast` golden corpus (replacing the M2 corpus
in-place, per Q2 Option A), and ~15 gtest unit cases. Total
**~100–120 fixtures** at M3 — comparable to M2's ~85–100 because
the per-`Sn` pass+fail pairs are uniformly small (one focused
construct per fixture).

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-check after Phase 1 design.*

Evaluation against Constitution v1.5.0 (in `.specify/memory/constitution.md`):

| Principle | Applies to M3? | Status | Notes |
|---|---|---|---|
| **I. Spec Is Authoritative** | **Yes — load-bearing** | ✅ | M3 implements `lang.ebnf` lines 826–1009 (`S1`–`S29` semantic constraints) verbatim. **No new `Sn`/`Nn`/`Pn` numbering** — every constraint is from the existing monotonic set. The constraint-table footnote (spec FR-010) explicitly cites `nsl_lang.ebnf:<line>` per row so the implementation/spec coupling is mechanically auditable. The 6 constructive `Sn` shipping paired-introspection per Clarifications Q1 → Option B do NOT amount to a spec change: the Constitution VIII "diagnostic-string" clause is by construction inapplicable to constraints that produce no diagnostic, so this is a literal-VIII honoring choice, not a carve-out. |
| **II. Layered Library Architecture** | **Yes — load-bearing** | ✅ | One layer (6) instantiated via M0's `add_nsl_library` macro. `nsl-sema` `DEPENDS nsl-ast nsl-basic` only — no edge to `nsl-parse`, `nsl-dialect`, or anything below (Principle II downward-only flow; FR-002/FR-003). The `nsl-sema` three-header layout (`Sema.h`, `SymbolTable.h`, `TypeSystem.h`) follows the same architectural pattern as `nsl-ast`'s per-node-kind exception, but is NOT covered by Principle II §3's verbatim wording — see Phase 0 research entry "Sema header layout" for the resolution. The driver `tools/nslc/main.cpp` stays ≤ 60 lines — `-emit=ast` already exists at M2; M3 adds NO new switch case. The Sema-execution glue lives in `lib/Driver/Sema.cpp` (small, ~30 lines) so the driver-binary discipline is preserved. **Tooling reuse**: the public `SymbolTable.h` + `TypeSystem.h` surface is what T2/T3/T6 consume — Principle II's "all tools reuse `libNSLFrontend.a`" satisfied by construction. |
| **III. Stock CIRCT Below `nsl` Dialect** | Forward-looking | ✅ | M3 ships no dialect or CIRCT-adjacent code. |
| **IV. Source-Locating Diagnostics** | **Yes — load-bearing** | ✅ | Every `Symbol` carries a `SourceRange declLoc` (per design §6 lines 692–697 and FR-005); every Sema diagnostic crosses through M1's `DiagnosticEngine` (FR-014) and renders to `path:line:col` in the canonical form. Per FR-011, every diagnostic message MUST cite `(SNN)` so a reader can `grep` from the failure back to `nsl_lang.ebnf:<line>`. `#line` survives unchanged from M2 — Sema does not consume or re-emit it (the AST it walks has already had `line_marker`s pruned at parse, FR-015). The `--diagnostic-format=json` plumbing inherited from M1 covers Sema diagnostics without modification. |
| **V. Inspectable, Deterministic Pipeline** | **Yes — gating** | ✅ | M3 does NOT add a new `-emit=` flag (per Clarifications Q2 → Option A; FR-021) — the existing `-emit=ast` covers both pre-Sema and post-Sema printer modes. **Determinism**: byte-stable AST-printer output across two builds (FR-029, SC-003); no pointer-leaks (FR-031); deterministic collection iteration in the printer (FR-030); no env-var influence other than `NSL_INCLUDE` (inherited from M1/M2); no embedded timestamps; no hash-map-iteration-derived order. The `TypeSystem` interner uses `llvm::DenseMap` for lookup ONLY; the printer iterates a sorted view of the keys (Principle V's "no hash-map-iteration-derived order" exception covers this). |
| **VI. Layered Test Discipline** | **Yes — NON-NEGOTIABLE** | ✅ | Sema tests are "one test per `S1`–`S29`" per Principle VI's "Sema tests" bullet (line 243). Per-`Sn` pass + fail fixture pairs (FR-023) collectively exercise the constraint surface; multi-error recovery fixtures (FR-025) exercise hybrid recovery (Q3 Option C); per-scope resolution fixtures (FR-026) and per-`Expr`-form width fixtures (FR-027) exercise the resolution + width-inference subsystems. lit + FileCheck for diagnostic-string-bearing fixtures (the 23 error/warning `Sn`); gtest for unit-level introspection assertions on the 6 constructive `Sn` (per Clarifications Q1 → Option B) and for `SymbolTable` / `TypeSystem` invariants. **Audited-project gate** (Principle VI's seven projects): forward-looking, gates M7. M3 leaves CI's stage 5 (e2e) and stage 6 (formal) wired-but-empty unchanged. |
| **VII. Spec ↔ Design Coupling** | **Yes** | ✅ | M3 implements `lang.ebnf §S1–S29` and `nsl_compiler_design.md` §§6 (SymbolTable) + §6.x (TypeSystem) verbatim; **no edits to `docs/spec/*.ebnf` are required by this plan**. Three design-doc actions (per Phase 0 research): (a) `docs/design/nsl_compiler_design.md` §6 lines 688–795 are implemented as written — no edits planned; (b) the `docs/CLAUDE.md` quick-map for `Sn` (§5) lists every `S1`–`S29` by file:line — if any line numbers shift during the M3 patch (unlikely; `nsl_lang.ebnf` is stable from M2), the same patch updates `docs/CLAUDE.md` §§4–7 per Principle VII's line-range rule; (c) the `CLAUDE.md` (project root) §1 NSL-feature roll-up gains a M3-era footnote naming the 6 `Sn` that ship paired-introspection (per Clarifications Q1 → Option B; spec Assumptions §"Constructive-shape Sn"); this footnote is the auditable record of the carve-out. |
| **VIII. Test-First Development** | **Yes — NON-NEGOTIABLE, gating** | ✅ | FR-028 codifies the per-fixture TDD discipline. Tasks plan will sequence each behavior as: (1) test-author commit (observed failing on then-current tree) → (2) implementation commit (test passes). The 58 per-`Sn` fixtures (29 × pass + fail) AND the 23 diagnostic-message-string assertions are the test-first artifacts; the 23 corresponding constraint checkers and the 6 constructive checkers follow them. The pre-M7 carve-out for refactor exemption (Principle VIII) applies — the Verilog-diff condition (d) is vacuous. **The Principle VIII diagnostic-string clause is satisfied for the 23 error/warning rows** (every fail fixture asserts the literal message, FR-015) **AND honored vacuously for the 6 constructive rows** (those produce no diagnostic; the introspection-expected-value flip serves the same Red→Green role per Clarifications Q1 → Option B). |
| **IX. Continuous Integration & Delivery** | **Yes** | ✅ | M0 wired the 6-stage pipeline; M1 / M2 filled stages 3 + 4 with lex/preprocess/parse content. M3 grows stage 3 (Unit & layer tests) with the 58 per-`Sn` fixtures + multi-error recovery + resolution + width corpora, and stage 4 (Lowering tests via lit + FileCheck) with the re-cut `-emit=ast` golden. Stages 5 (end-to-end) and 6 (formal) remain wired-but-empty (gated to M7 / M8). The local-reproduction `scripts/ci.sh` continues to be the single authoritative entry point. **The Principle IX transitional clause was retired in commit `3b6decc` (Constitution v1.5.0)** — green CI is a hard merge gate for M3's PR. |
| **Build/Code/Licensing Standards** | **Yes** | ✅ | C++17 enforced by M0's `target_compile_features` + `set(CMAKE_CXX_EXTENSIONS OFF)`. LLVM/CIRCT conventions throughout (`llvm::DenseMap`, `llvm::SmallVector`, `llvm::StringRef`, `llvm::ArrayRef` per design §6 / §6.x). Apache-2.0 WITH LLVM-exception SPDX header on every new file (M0's `check_spdx.py` runs against `git ls-files`; SC-008). |
| **Development Workflow** | Yes | ✅ | This plan was drafted via `/speckit-specify` → `/speckit-clarify` → `/speckit-plan`. AI-attribution per `CONTRIBUTING.md` §5. |
| **External Integrations** (Linear / GitHub Issues / CodeRabbit) | Yes | ✅ | M3 work tracked under Linear `NSL-<N>` (feature-track; team prefix `NSL` per memory). CodeRabbit gate applies. No project-level integration changes. |
| **Governance — Milestone Plan** | Yes | ✅ | M3 follows M2 directly per `README.md` §Roadmap. No milestone renumbering. No constitution amendment required (the Q1 paired-introspection carve-out is honored *vacuously* under Principle VIII rather than via amendment). |

**Gate result: PASSES** on first evaluation. No violations to record in the Complexity Tracking section.

## Project Structure

### Documentation (this feature)

```text
specs/006-m3-sema/
├── plan.md                                  # this file
├── spec.md                                  # /speckit-specify + /speckit-clarify output
├── research.md                              # Phase 0 — every Technical Context decision justified
├── data-model.md                            # Phase 1 — Symbol + Type entities mirroring nsl_compiler_design.md §6
├── quickstart.md                            # Phase 1 — clone → build → exercise per-Sn fixture + post-Sema -emit=ast
├── contracts/                               # Phase 1 — interface contracts
│   ├── sema-api.contract.md                 # public-header surface invariants (Sema.h / SymbolTable.h / TypeSystem.h)
│   ├── sema-stability.contract.md           # determinism, identity, ownership invariants across patches
│   ├── emit-ast-format.contract.md          # post-Sema -emit=ast text format frozen at M3 (Q2 Option A)
│   └── diagnostic-string.contract.md        # diagnostic-message text contract for the 23 error/warning Sn
├── checklists/
│   └── requirements.md                      # /speckit-specify + /speckit-clarify validation
└── tasks.md                                 # /speckit-tasks output (NOT created by /speckit-plan)
```

### Source Code (repository root)

```text
nslc/
├── include/nsl/
│   └── Sema/                                # M3 populates (M0 created empty dir)
│       ├── Sema.h                           # NEW — Sema entry-point + SemaResult
│       ├── SymbolTable.h                    # NEW — Symbol hierarchy + Scope stack + lookup API
│       └── TypeSystem.h                     # NEW — Type hierarchy + TypeRef interner
├── lib/
│   ├── Sema/
│   │   ├── CMakeLists.txt                   # MODIFIED — list sources via add_nsl_library
│   │   ├── Sema.cpp                         # NEW — top-level Sema::run(); pass orchestration
│   │   ├── SymbolTable.cpp                  # NEW — scope-stack + declare/lookup/lookupScoped impls
│   │   ├── TypeSystem.cpp                   # NEW — interning of BitVector/Struct/Memory; equal()
│   │   ├── ResolutionPass.cpp               # NEW — single top-down pass: name-resolve + width-inference (Q3 Option C)
│   │   └── Constraints/
│   │       ├── S01_NoDoubleUnderscore.cpp   # NEW — S1
│   │       ├── S02_WireNoInit.cpp           # NEW — S2
│   │       ├── S03_AssignmentLHSKind.cpp    # NEW — S3 (with FixItHint)
│   │       ├── S04_FuncDummyArgDirs.cpp     # NEW — S4
│   │       ├── S05_ReturnTerminalDir.cpp    # NEW — S5
│   │       ├── S06_ProcArgRegOnly.cpp       # NEW — S6
│   │       ├── S07_SeqInsideFuncProc.cpp    # NEW — S7 (with FixItHint)
│   │       ├── S08_LoopInsideSeq.cpp        # NEW — S8
│   │       ├── S09_ForVarReg.cpp            # NEW — S9
│   │       ├── S10_GenerateVarInteger.cpp   # NEW — S10
│   │       ├── S11_StateNameProcScoped.cpp  # NEW — S11
│   │       ├── S12_PartialLHSVariableOnly.cpp  # NEW — S12
│   │       ├── S13_AltAnyClassification.cpp # NEW — S13 (constructive; introspection only)
│   │       ├── S14_ConditionalElseRequired.cpp # NEW — S14 (with FixItHint)
│   │       ├── S15_SliceIndicesConst.cpp    # NEW — S15
│   │       ├── S16_ParamHDLOnly.cpp         # NEW — S16
│   │       ├── S17_SystemTaskSimulationOnly.cpp # NEW — S17
│   │       ├── S18_StructMSBFirstPacking.cpp   # NEW — S18 (constructive)
│   │       ├── S19_OneClockPerGoto.cpp      # NEW — S19 (constructive; introspection-only at M3, full M5/M6)
│   │       ├── S20_InterfaceModifierClkRst.cpp # NEW — S20
│   │       ├── S21_ProcMethodsFinishInvoke.cpp # NEW — S21
│   │       ├── S22_ReturnWidthMatch.cpp     # NEW — S22
│   │       ├── S23_RegOmittedWidth1Bit.cpp  # NEW — S23 (constructive)
│   │       ├── S24_MemPartialInitZero.cpp   # NEW — S24 (constructive)
│   │       ├── S25_GotoTwoKinds.cpp         # NEW — S25
│   │       ├── S26_FuncFunctionWarn.cpp     # NEW — S26 (warning only)
│   │       ├── S27_ControlTerminalAs1Bit.cpp # NEW — S27 (constructive; classifier)
│   │       ├── S28_FirstStatePositioning.cpp # NEW — S28
│   │       └── S29_InitBlockPlacement.cpp   # NEW — S29
│   └── Driver/
│       ├── CMakeLists.txt                   # MODIFIED — add Sema.cpp source
│       ├── Sema.cpp                         # NEW — Compilation::sema() implementation; ~30 lines
│       └── EmitAST.cpp                      # MODIFIED — call Sema after parse before printing
├── tools/nslc/main.cpp                      # UNCHANGED — no new switch case (Q2 Option A)
├── test/                                    # M3 grows the lit tree
│   └── sema/
│       ├── s01/ … s29/                      # NEW — one dir per Sn with pass.nsl + fail.nsl (29 × 2 = 58 files)
│       ├── recovery/                        # NEW — multi-error fixtures (FR-025 corpus + extras)
│       ├── resolution/                      # NEW — per-scope + per-symbol-kind name resolution (FR-026)
│       ├── width/                           # NEW — per-Expr-form width inference (FR-027)
│       └── emit-ast-resolved/               # NEW — post-Sema -emit=ast format goldens (re-cut from M2 in same patch)
├── test_unit/                               # M3 grows the gtest tree
│   ├── symbol_table_test/                   # NEW — scope stack invariants; declare/lookup/lookupScoped
│   ├── type_system_test/                    # NEW — interning equality; pointer-equality contract
│   ├── resolution_pass_test/                # NEW — no-cascade guarantee (FR-017)
│   └── constructive_sn_test/                # NEW — paired-introspection assertions for S13/S18/S19/S23/S24/S27 (Q1 Option B)
├── test/parse/grammar/                      # MODIFIED — re-cut goldens from M2 to reflect post-Sema printer enrichments (Q2 Option A)
├── scripts/
│   └── check_layering.py                    # MODIFIED — extend to forbid `nsl-sema`→`nsl-parse` link edges
├── README.md                                # POSSIBLY MODIFIED — Building/Status section gains post-Sema `-emit=ast` example
├── docs/CLAUDE.md                           # POSSIBLY MODIFIED — line-range refresh if any §§4–7 ranges shift (per Principle VII)
└── CLAUDE.md                                # MODIFIED — SPECKIT START/END marker → ./specs/006-m3-sema/plan.md; §1 footnote for the 6 constructive Sn
```

**Structure Decision**: Continues M0/M1/M2's compiler layout. The
single M3 layer populates `include/nsl/Sema/` and `lib/Sema/`. The
driver glue lives in `lib/Driver/Sema.cpp` (new) plus a small
modification to `lib/Driver/EmitAST.cpp` (call Sema after parse) —
no change to `tools/nslc/main.cpp`'s ≤60-line discipline (Q2
Option A). The test corpus splits cleanly: lit fixtures under
`test/sema/` (where the artifact under test is a textual diagnostic
or a textual `-emit=ast` golden) and gtest unit fixtures under
`test_unit/` (where the artifact is a C++ assertion on internal
state, including the introspection assertions for the 6
constructive `Sn`). The per-`Sn`-source-file pattern in
`lib/Sema/Constraints/` is the architecturally significant new
structure introduced by M3 — one source per constraint family
keeps the per-`Sn` checker focused, matches the per-`Sn` test
directory layout 1:1, and makes adding a future `Sn` (per
Constitution Principle I monotonic-numbering) a trivial
single-file PR (per spec SC-010).

## Complexity Tracking

> **Fill ONLY if Constitution Check has violations that must be justified**

No violations to track. Constitution Check (above) passes on first
evaluation. The post-design re-check at the end of `research.md`
records the same result.
