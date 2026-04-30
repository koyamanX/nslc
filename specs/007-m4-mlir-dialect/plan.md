<!-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception -->

# Implementation Plan: M4 — `nsl` MLIR Dialect (`nsl-dialect`)

**Branch**: `007-m4-mlir-dialect` | **Date**: 2026-04-30 | **Spec**: [spec.md](./spec.md)
**Input**: Feature specification from `/specs/007-m4-mlir-dialect/spec.md`

## Summary

Land the next compiler-track library — `nsl-dialect` (7) — defining
every `nsl.*` MLIR operation and `!nsl.*` type that subsequent
milestones (M5 lowering, M6 CIRCT, M7 end-to-end) consume. M3
delivered the resolved AST; M4 turns the dialect itself into the
architectural seam (Constitution Principle III) where NSL-specific
abstractions end and stock CIRCT begins. **M4 is the dialect-surface
freeze** — every op definition, every verifier hook, and the
`nsl-opt` round-trip contract become stable inputs for M5's AST→MLIR
lowering work.

Deliverables, all mandated by the spec (FR cross-references in
parens):

- **`nsl-dialect`** populates `include/nsl/Dialect/NSL/IR/` with a
  single umbrella public header `NSLDialect.h` that re-exports the
  TableGen-generated per-op classes. Concrete op classes mirror
  [`docs/design/nsl_compiler_design.md`](../../docs/design/nsl_compiler_design.md)
  §§7–10 verbatim — 41 named ops (`nsl.module`, `nsl.proc`,
  `nsl.transfer`, …, `nsl.structural_generate`, the 4 `nsl.sim_*`
  variants, the 3 markers `nsl.fire_probe` / `nsl.struct_cast` /
  `nsl.field`) plus auto-generated implicit-terminator ops for
  region-bearing parents. Three dialect types
  (`!nsl.bits<N>`, `!nsl.struct<@T>`, `!nsl.mem<[D x T]>`) round-trip
  via `useDefaultTypePrinterParser = 1`. FR-001, FR-006, FR-007,
  FR-009, FR-010.
- **TableGen + ODS sources** (`lib/Dialect/NSL/IR/NSL*.td`)
  declare the dialect, the 41 ops, the 3 types, and every TableGen-
  expressible structural trait (`Symbol`, `SymbolTable`,
  `HasParent<...>`, `SingleBlockImplicitTerminator<...>`,
  `SameOperandsElementType`, `SameOperandsShape`). Standard CMake
  helpers (`add_mlir_dialect`, `add_mlir_doc`, `mlir_tablegen`)
  generate the per-op headers + `.cpp.inc` glue. FR-002, FR-006.
- **Hand-written verifier bodies** (`lib/Dialect/NSL/IR/NSLOps.cpp`)
  implement the **structural-only** scope resolved in Clarifications
  session 2026-04-30 Q1 → Option A: `LogicalResult <Op>::verify();`
  for every op whose structural invariants exceed what
  trait-declarative TableGen can express. Per Q2 Option B, **rows in
  spec FR-013 marked `parent (transitively) = X` use a hand-written
  ancestor-walk** (`op->getParentOp()` upward until kind X is found
  or the top is reached); rows marked `parent = X` use the standard
  `HasParent<X>` trait. Affects ~5 ops (`nsl.while`, `nsl.for`,
  `nsl.finish`, `nsl.goto` label-form, plus any future transitive-
  parent op). FR-011, FR-012, FR-013.
- **`nsl-opt` developer/test binary** (`tools/nsl-opt/main.cpp`)
  links `nsl-dialect` + the CIRCT dialects + upstream `MLIROptLib`
  (the `MlirOptMain` driver) and registers the `nsl` dialect via the
  registration entry-point exported by `NSLDialect.h`. Zero passes
  registered at M4 (passes are M5+). Classified as a developer/test
  tool per Constitution Principle II §4 — NOT a user-facing T-track
  deliverable. FR-014, FR-015, FR-016.
- **Driver dialect-load wiring**: `lib/Driver/Compilation.cpp` gains
  the `mlirCtx_.loadDialect<nsl::dialect::NSLDialect>()` call site
  per design §11 line 1145, AND the `lowerToNSL` /
  `runNSLPasses` member-function declarations whose **bodies** are
  trivial diagnostic stubs ("MLIR lowering not yet implemented; see
  M5") at M4. The `nslc` driver's `-emit=*` choice list remains
  exactly `{tokens, ast}` (FR-023); `-emit=mlir` lands at M5.
  FR-004, FR-022, FR-024.
- **Test corpus** under `test/Dialect/` — Constitution Principle VI's
  "**Dialect tests use `nsl-opt` for round-trip verification of
  `.mlir`**" (line 350) applied: 40 round-trip pass fixtures
  (one `<op>_roundtrip.mlir` per op in spec FR-010), 3 type-round-
  trip fixtures (`!nsl.bits` / `!nsl.struct` / `!nsl.mem`), and ~50
  invalid fixtures (one `<op>_invalid_<reason>.mlir` per cell in
  spec FR-013 with ≥ 1 invariant). Plus a CI guard
  (`scripts/check_dialect_coverage.py`, new) verifying the per-op
  fixture-existence invariant per FR-021. FR-017 through FR-021.

Two /speckit-clarify decisions (session 2026-04-30) frame the
scope: **Q1** — verifier strictness is structural-only (no Sema
re-checks); **Q2** — immediate-parent rows use TableGen `HasParent`,
transitive-parent rows use hand-written ancestor-walk. M4's
implementation latitude is in *how* the TableGen sources are
partitioned and *how* the verifier bodies are factored, not *what*
they accept or reject.

## Technical Context

**Language/Version**: **C++17** across the verifier glue, the
`nsl-opt` driver, and the small driver-stub additions in
`lib/Driver/` (Constitution Build/Code/Licensing — C++20 forbidden
until amendment). **TableGen** (LLVM's domain-specific .td language;
no version pin separate from LLVM/MLIR — comes with the toolchain)
for the dialect / op / type declarations. Helper scripts in **Python
3.8+** matching M0/M1/M2/M3 baseline.
**Primary Dependencies**: **LLVM + MLIR** at the CIRCT-pinned commit
(vendored prebuilt; M0 §2). Specifically the `MLIRIR`, `MLIRSupport`,
`MLIRDialect`, `MLIRPass`, and `MLIROptLib` libraries plus the
`mlir-tblgen` tool. The CIRCT dialects (`hw`, `comb`, `seq`, `fsm`,
`sv`) are loaded by `nsl-opt`'s registry (per design §11 lines
1146–1150) so that mixed-dialect fixtures parse and round-trip; the
M4 dialect itself does NOT reference CIRCT types or ops (the NSL
→ CIRCT lowering is M6's job). M3's `nsl-basic`
(`SourceManager`, `SourceRange`) is consumed via its public header
exclusively. **No new external dependencies introduced at M4.**
**Storage**: N/A. The dialect's MLIR ops + types live in-memory
within the `mlir::MLIRContext`'s per-dialect storage; ownership
follows MLIR's standard arena-style allocation. No persistent IR
artifact at M4 (the `.mlir` text form is the only serialization,
produced by `nsl-opt`'s printer).
**Testing**: **lit + FileCheck** (`test/Dialect/`) for the 35
per-op round-trip pass fixtures, the 3 per-type round-trip fixtures,
and the ~50 invalid-fixture cases. Per Constitution Principle VI's
"Dialect tests use `nsl-opt` for round-trip verification of `.mlir`"
(line 350) — lit + FileCheck is the canonical driver. lit's
`// expected-error{{<substring>}}` syntax for invalid fixtures
(MLIR's standard mechanism). **GoogleTest** (`test_unit/`) for one
small unit test of the dialect's registration-entry-point
idempotency (`registerNSLDialect()` called twice on the same
registry should be a no-op) — the only C++-level invariant that
isn't expressible via lit. Per-fixture pass+fail discipline
(Principle VIII) for every op + invariant.
**Target Platform**: **Linux x86_64** (M0/M1/M2/M3 baseline). Other
architectures and operating systems remain deferred.
**Project Type**: Compiler middle-end — extends M3's six-layer
front-end with a seventh (`nsl-dialect`) and an `nsl-opt`
developer/test binary. No new CLI flag in `nslc` at M4 (per scope
resolution; FR-022/FR-023). The driver gains a dialect-load call
site + two stub-bodies for `Compilation::lowerToNSL` /
`runNSLPasses` whose real bodies land at M5.
**Performance Goals**: `nsl-opt` round-trip on a representative
fixture (≤ 200 lines of MLIR) finishes in **< 1 s** on the reference
host (informal; round-trip is dominated by MLIR's parser/printer,
not by NSL-specific verifier code; no per-line throughput SLO at M4
— deferred to M7's audited-corpus regression). Verifier overhead per
op is expected to be modest — TableGen-trait checks are
constant-time; the ~5 hand-written ancestor-walks are linear in op
nesting depth (typically ≤ 6 for representative NSL).
**Constraints**: **Byte-stable IR output** (FR-025, Principle V) —
two `nsl-opt` invocations on the same input + flag list MUST produce
byte-identical stdout, including across Debug/Release builds and
gcc/clang compilers. **Symbol identity not pointer-leaked** (FR-027)
— every printed `@<name>` resolves by symbol name, not by
`Operation*` pointer; MLIR's standard `SymbolRefAttr` machinery
satisfies this. **Deterministic collection iteration** (FR-026) —
no `unordered_map` / unordered `DenseMap` iteration in any printer
or diagnostic-producing path; MLIR's `Operation::getRegions()` /
`Block::getOps()` are insertion-ordered and satisfy this. **Layered
structure preserved** (FR-001, FR-003, FR-005) — `nsl-dialect`
DEPENDS only on `nsl-basic` + upstream MLIR `IR` / `Support`; CI's
`scripts/check_layering.py` (introduced at M2, extended at M3,
extended again at M4 to forbid `nsl-dialect`→`nsl-ast` /
`nsl-sema` / `nsl-parse` / `nsl-lex` / `nsl-preprocess` edges).
**Scale/Scope**: 1 library (1 .td file or 3 partitioned per FR-002:
`NSLDialect.td` + `NSLOps.td` + `NSLTypes.td` — see Phase 0
research; ~2k LOC TableGen total + ~800 LOC hand-written C++ for
verifier bodies + dialect init + 1 umbrella header). 1 binary
(`tools/nsl-opt/main.cpp`, ~50 lines). 2 driver-stub source files in
`lib/Driver/` (~30 lines each). Fixture count: **35 per-op
round-trip fixtures** (FR-017), **3 per-type round-trip fixtures**
(FR-018), **~50 per-invariant invalid fixtures** (FR-019; cardinality
per Q1 Option A — structural-only). Total **~88 dialect fixtures**
at M4. The CI coverage-guard script
(`scripts/check_dialect_coverage.py`) is ~80 lines — enumerates
registered op classes and asserts paired-fixture existence.

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-check after Phase 1 design.*

Evaluation against Constitution v1.7.0 (in `.specify/memory/constitution.md`):

| Principle | Applies to M4? | Status | Notes |
|---|---|---|---|
| **I. Spec Is Authoritative** | **Yes** | ✅ | M4 implements `lang.ebnf` §§4–11 (declare / module / internal-structure / definitions / actions / atomic / system-tasks / expressions) **as the dialect's op set**, mirroring `docs/design/nsl_compiler_design.md` §§7–10. **No new `Sn`/`Nn`/`Pn` numbering** — the spec is unchanged at M4. The "no silent AST drops" sub-clause (Principle I, v1.7.0) is **vacuously satisfied** at M4 because the AST→MLIR lowering is M5's work; M4 ships only the dialect (op shapes + verifiers + round-trip), with no AST-consuming code. The spec FR-010 op table cites design §7 line numbers per row so the implementation/design coupling is mechanically auditable per Principle VII. |
| **II. Layered Library Architecture** | **Yes — load-bearing** | ✅ | One layer (7) instantiated via M0's `add_nsl_library` macro plus MLIR's standard `add_mlir_dialect` helper. `nsl-dialect` `DEPENDS nsl-basic` + upstream MLIR `IR`/`Support` only — no edge to `nsl-ast` / `nsl-sema` / `nsl-parse` / `nsl-lex` / `nsl-preprocess` (Principle II downward-only flow; FR-001/FR-003/FR-005). The `nsl-dialect` header layout uses **a single public umbrella header** (`NSLDialect.h`) — **NOT** an exception under Principle II §3 (which names `nsl-ast` and `nsl-sema` only). The TableGen-generated per-op headers (`NSLOps.h.inc`, etc.) are private build artifacts under the library's internal include path; consumers reach them only through the umbrella. The driver `tools/nslc/main.cpp` stays ≤ 60 lines (no change at M4). The new `nsl-opt` binary at `tools/nsl-opt/main.cpp` is a **developer/test tool** per Principle II §4 — NOT a user-facing T-track deliverable; release tarballs MUST NOT bundle it (FR-016). T-track binaries (`nsl-lsp`, `nsl-fmt`, `nsl-lint`) consume `libNSLFrontend.a` and do NOT consume the dialect at M4 — Principle II's "all tools reuse `libNSLFrontend.a`" satisfied by construction. |
| **III. Stock CIRCT Below `nsl` Dialect** | **Yes — load-bearing** | ✅ | M4 IS the `nsl` dialect — the architectural seam where Principle III applies. The spec **explicitly excludes** the NSL → CIRCT lowering passes (M6) and the structural-expansion passes (M5) from M4 scope. No hand-rolled netlist, RTL, register-inference, or state-machine-lowering passes are introduced at M4. The CIRCT dialects are loaded by `nsl-opt` (FR-014) only so mixed-dialect fixtures parse correctly; they are NOT a build-time dependency of `nsl-dialect` itself (the dialect's TableGen sources reference no CIRCT types). |
| **IV. Source-Locating Diagnostics** | **Yes — load-bearing** | ✅ | Every `nsl.*` op carries `mlir::Location` per upstream MLIR convention; verifier diagnostics route through `op->emitOpError(...)` which renders `path:line:col` per Principle IV (FR-012). For hand-written `.mlir` fixtures the location resolves to the `.mlir` text-position; for AST-built MLIR (M5+) the location resolves to the originating NSL `SourceRange` encoded as `FileLineColLoc` per design §12 line 1211. **`#line` survives unchanged from M3** — the M4 dialect operates on already-resolved-by-Sema input and is never the consumer of `#line` directives. The diagnostic-format-text policy at M4 deliberately differs from M3's `Sn` model: dialect-verifier diagnostic text is **substring-matched** in fixtures (FileCheck `// expected-error{{<substring>}}`), not literal-asserted, because the message wording follows MLIR upstream conventions and re-stating those with frozen text would defeat the convention (per Assumptions paragraph in spec). This is a **deliberate carve-out** from Principle VIII's `Sn`/`Nn`/`Pn` clause documented in spec Assumptions — applied because dialect invariants are NOT NSL-spec semantic constraints. |
| **V. Inspectable, Deterministic Pipeline** | **Yes — gating** | ✅ | M4 does NOT add a new `-emit=` flag in `nslc` (per FR-022/FR-023; `-emit=mlir` lands at M5). The new `nsl-opt` binary IS the M4 inspectability surface: stdin/stdout/file-arg per upstream `MlirOptMain` (FR-015). **Determinism**: byte-stable `nsl-opt` output across two builds (FR-025); no pointer-leaks (FR-027); deterministic collection iteration in printer (FR-026); no env-var influence; no embedded timestamps. The TableGen-generated parsers/printers use MLIR's deterministic `SmallVector`/insertion-ordered iteration for op regions and operand lists. |
| **VI. Layered Test Discipline** | **Yes — NON-NEGOTIABLE** | ✅ | "Dialect tests use `nsl-opt` for round-trip verification of `.mlir`" (Principle VI line 350) — M4 implements this verbatim. Per-op round-trip fixtures (FR-017) collectively exercise the 35-op surface; per-type round-trip fixtures (FR-018) cover the 3 type forms; per-invariant invalid fixtures (FR-019) exercise the structural-verifier surface (Q1 Option A; ~50 fixtures). lit + FileCheck for round-trip and invalid; gtest for the dialect-registry idempotency unit case. **Audited-project gate** (Principle VI's seven projects): forward-looking, gates M7. M4 leaves CI's stage 5 (e2e) and stage 6 (formal) wired-but-empty unchanged. |
| **VII. Spec ↔ Design Coupling** | **Yes** | ✅ | M4 implements `nsl_compiler_design.md` §§7–10 verbatim; **no edits to `docs/spec/*.ebnf` are required** by this plan (the spec is unchanged at M4). Three design-doc actions: (a) `docs/design/nsl_compiler_design.md` §7 lines 882–931 are implemented as written — no edits planned for the op summary itself; (b) **A small consolidation note** is added to design §7 in the M4 patch documenting that the marker / lowering-helper ops (`nsl.fire_probe`, `nsl.struct_cast`, `nsl.field`, `nsl.case`, `nsl.default`, `nsl.goto`, `nsl.structural_generate`) introduced in §§8–10 are dialect ops shipping at M4, not §7 omissions — this is the single Principle VII "design doc was incomplete; the M4 implementation surfaces a gap" action; (c) `docs/CLAUDE.md` §6 (compiler-design TOC) line ranges are kept current if the §7 consolidation note shifts boundaries. The project-root `CLAUDE.md` §1 NSL-feature roll-up's "Lower to dialect" column entries already cite M4 by op name (`nsl::ModuleOp`, `nsl::DeclareOp` shorthand, etc.); no edits required. |
| **VIII. Test-First Development** | **Yes — NON-NEGOTIABLE, gating** | ✅ | FR-020 codifies the per-fixture TDD discipline (parallel to M3's FR-028). Tasks plan will sequence each behavior as: (1) test-author commit (observed failing on then-current tree because the op is undefined or the verifier is unimplemented) → (2) implementation commit (TableGen + verifier glue lands; test passes). The 40 round-trip fixtures + 3 type fixtures + ~50 invalid fixtures are the test-first artifacts; the corresponding TableGen records and verifier bodies follow them. The pre-M7 carve-out for refactor exemption (Principle VIII condition d, the Verilog-diff condition) is **vacuous at M4** because M5's `-emit=verilog` end-to-end pipeline is forward-looking. **The Principle VIII diagnostic-string clause** is honored under a **deliberate dialect-layer carve-out** documented in spec Assumptions: substring-match instead of literal-string-match, because dialect verifier wording follows MLIR upstream conventions, not NSL-spec conventions. The spec's Assumptions paragraph and FR-012 frame this carve-out; the Constitution does not need amendment because Principle VIII's clause specifically covers `Sn`/`Nn`/`Pn` (NSL-spec constraints), not MLIR-layer structural rules. |
| **IX. Continuous Integration & Delivery** | **Yes** | ✅ | M0 wired the 6-stage pipeline; M1/M2/M3 filled stages 3 + 4 with lex/preprocess/parse/sema content. M4 grows stage 3 (Unit & layer tests) with the dialect round-trip + invalid corpora, and stage 4 (Lowering tests via lit + FileCheck) with `nsl-opt`-driven `.mlir` round-trip and verifier-reject tests. Stages 5 (end-to-end) and 6 (formal) remain wired-but-empty (gated to M7/M8). The local-reproduction `scripts/ci.sh` continues to be the single authoritative entry point. **The Principle IX transitional clause was retired at v1.5.0** — green CI is a hard merge gate for M4's PR. |
| **Build/Code/Licensing Standards** | **Yes** | ✅ | C++17 enforced by M0's `target_compile_features` + `set(CMAKE_CXX_EXTENSIONS OFF)`. LLVM/MLIR conventions throughout (`mlir::Op`, `mlir::OpBuilder`, `mlir::Location`, `mlir::SymbolRefAttr`, the standard CMake helpers). Apache-2.0 WITH LLVM-exception SPDX header on every new file (M0's `check_spdx.py` runs against `git ls-files`; SC-010). TableGen `.td` files MUST carry the SPDX header in the file's leading multi-line C-style `/* … */` comment block — verified by the same script. |
| **Development Workflow** | Yes | ✅ | This plan was drafted via `/speckit-specify` → `/speckit-clarify` → `/speckit-plan`. AI-attribution per `CONTRIBUTING.md` §5. |
| **External Integrations** (Linear / GitHub Issues / CodeRabbit) | Yes | ✅ | M4 work tracked under Linear `NSL-<N>` (feature-track; team prefix `NSL` per memory). CodeRabbit gate applies. No project-level integration changes. |
| **Governance — Milestone Plan** | Yes | ✅ | M4 follows M3 directly per `README.md` §Roadmap. No milestone renumbering. No constitution amendment required (the dialect-verifier substring-match policy is a Principle IV/VIII *application detail* documented in spec Assumptions, not a constitutional carve-out). |

**Gate result: PASSES** on first evaluation. No violations to record in the Complexity Tracking section.

## Project Structure

### Documentation (this feature)

```text
specs/007-m4-mlir-dialect/
├── plan.md                                  # this file
├── spec.md                                  # /speckit-specify + /speckit-clarify output
├── research.md                              # Phase 0 — every Technical Context decision justified
├── data-model.md                            # Phase 1 — Op + Type entities mirroring nsl_compiler_design.md §§7–10
├── quickstart.md                            # Phase 1 — clone → build → exercise round-trip + invalid fixture
├── contracts/                               # Phase 1 — interface contracts
│   ├── dialect-api.contract.md              # public-header surface (NSLDialect.h) + registration entry-point
│   ├── dialect-stability.contract.md        # determinism, identity, ordering invariants across patches
│   ├── nsl-opt-cli.contract.md              # nsl-opt CLI surface — flags, exit codes, stdin/stdout shape
│   └── verifier-diagnostic.contract.md      # diagnostic-message shape + substring-match policy (Q1 carve-out)
├── checklists/
│   └── requirements.md                      # /speckit-specify + /speckit-clarify validation
└── tasks.md                                 # /speckit-tasks output (NOT created by /speckit-plan)
```

### Source Code (repository root)

```text
nslc/
├── include/nsl/
│   └── Dialect/NSL/IR/                      # M4 populates (M0 created empty dir)
│       └── NSLDialect.h                     # NEW — umbrella public header; re-exports TableGen-generated headers; declares registerNSLDialect()
├── lib/
│   ├── Dialect/
│   │   └── NSL/
│   │       └── IR/
│   │           ├── CMakeLists.txt           # NEW — add_mlir_dialect + add_nsl_library + tablegen invocations
│   │           ├── NSLDialect.td            # NEW — dialect class, namespace, useDefaultTypePrinterParser
│   │           ├── NSLOps.td                # NEW — 41 ops + auto-generated terminators in one file (per Phase 0 research, post-Q6)
│   │           ├── NSLTypes.td              # NEW — 3 types (!nsl.bits, !nsl.struct, !nsl.mem)
│   │           ├── NSLDialect.cpp           # NEW — dialect-init code; type printer/parser glue
│   │           ├── NSLOps.cpp               # NEW — verifier bodies for 41 ops (TableGen HasParent for immediate; hand-walk for transitive per Q2 Option B)
│   │           └── NSLTypes.cpp             # NEW — type-class glue (init, hash, equal)
│   └── Driver/
│       ├── CMakeLists.txt                   # MODIFIED — add Compilation.cpp dialect-load line; add LowerToNSL.cpp + RunNSLPasses.cpp stub sources
│       ├── Compilation.cpp                  # MODIFIED — add mlirCtx_.loadDialect<nsl::dialect::NSLDialect>() call site (per design §11 line 1145)
│       ├── LowerToNSL.cpp                   # NEW — Compilation::lowerToNSL stub body emits "MLIR lowering not yet implemented; see M5" (FR-004)
│       └── RunNSLPasses.cpp                 # NEW — Compilation::runNSLPasses stub body emits same diagnostic
├── tools/
│   ├── nslc/                                # UNCHANGED — driver flag set unchanged at M4 (FR-022/FR-023)
│   │   └── main.cpp
│   └── nsl-opt/                             # NEW — developer/test binary per Principle II §4
│       ├── CMakeLists.txt                   # NEW — link nsl-dialect, MLIROptLib, MLIRIR, MLIRSupport, CIRCT dialect libs
│       └── main.cpp                         # NEW — calls MlirOptMain; registers nsl + CIRCT dialects (FR-014)
├── test/
│   └── Dialect/                             # M4 grows the lit tree
│       ├── module-level/                    # NEW — round-trip + invalid fixtures: nsl.module, nsl.struct, nsl.submodule, nsl.connect
│       ├── storage/                         # NEW — nsl.reg, nsl.wire, nsl.variable, nsl.mem
│       ├── control-terminal/                # NEW — nsl.func_in, nsl.func_out, nsl.func_self
│       ├── action-block/                    # NEW — nsl.alt, nsl.any, nsl.if, nsl.parallel, nsl.seq, nsl.while, nsl.for
│       ├── action-helper/                   # NEW — nsl.case, nsl.default
│       ├── atomic/                          # NEW — nsl.transfer, nsl.clocked_transfer, nsl.incdec, nsl.call, nsl.finish, nsl.finish_method, nsl.invoke_method
│       ├── procedure/                       # NEW — nsl.proc, nsl.first_state, nsl.state, nsl.func
│       ├── procedure-helper/                # NEW — nsl.goto
│       ├── system-task/                     # NEW — nsl.sim_display, nsl.sim_finish, nsl.sim_init, nsl.sim_delay
│       ├── marker/                          # NEW — nsl.fire_probe, nsl.struct_cast, nsl.field
│       ├── expansion-only/                  # NEW — nsl.structural_generate
│       └── Types/                           # NEW — !nsl.bits, !nsl.struct, !nsl.mem round-trip fixtures
├── test_unit/
│   └── dialect_register_test/               # NEW — 1 gtest case: registerNSLDialect() idempotency
├── scripts/
│   ├── check_layering.py                    # MODIFIED — extend to forbid nsl-dialect→{nsl-ast, nsl-sema, nsl-parse, nsl-lex, nsl-preprocess} link edges
│   └── check_dialect_coverage.py            # NEW — enumerate registered op classes; assert per-op fixture-existence per FR-021
├── README.md                                # POSSIBLY MODIFIED — Building/Status section gains nsl-opt round-trip example
├── docs/design/nsl_compiler_design.md       # MODIFIED — §7 consolidation note for the §§8–10-introduced ops (Principle VII; SC-009)
├── docs/CLAUDE.md                           # POSSIBLY MODIFIED — §6 line ranges if §7 consolidation note shifts boundaries (per Principle VII)
└── CLAUDE.md                                # MODIFIED — SPECKIT START/END marker → ./specs/007-m4-mlir-dialect/plan.md
```

**Structure Decision**: Continues M0/M1/M2/M3's compiler layout. The
single M4 layer populates `include/nsl/Dialect/NSL/IR/` and
`lib/Dialect/NSL/IR/` — note the deeper directory nesting than the
front-end layers (`Dialect/NSL/IR/`) which matches CIRCT's
convention (e.g., `circt/include/circt/Dialect/HW/IR/`). The driver
gains two stub source files (`LowerToNSL.cpp`, `RunNSLPasses.cpp`)
plus a one-line modification to `Compilation.cpp` — no change to
`tools/nslc/main.cpp`'s ≤60-line discipline. The new
`tools/nsl-opt/` directory is the M4 binary surface; it's parallel
to `tools/nslc/` but is classified as a developer/test tool per
Principle II §4 (release rule deferred). The test corpus splits
cleanly: lit fixtures under `test/Dialect/<category>/` (where the
artifact under test is `.mlir` text or a substring-match diagnostic)
and gtest under `test_unit/dialect_register_test/` (where the
artifact is a C++ assertion on dialect-registry state). The
per-category subdirectory pattern in `test/Dialect/` is the
architecturally significant new structure introduced by M4 — one
subdirectory per FR-010 op category keeps fixtures grouped, matches
the design §7 op summary's section layout 1:1, and makes adding a
future op (per Constitution Principle I monotonic-numbering of the
op set) a trivial single-fixture-pair PR (per spec SC-012).

## Complexity Tracking

> **Fill ONLY if Constitution Check has violations that must be justified**

No violations to track. Constitution Check (above) passes on first
evaluation. The post-design re-check at the end of `research.md`
records the same result.
