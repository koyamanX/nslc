<!-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception -->

# Implementation Plan: M7 — `nsl-driver` end-to-end (`nslc -emit=verilog`); P-VEN vendoring + P-VCD golden VCDs + audited-corpus regression

**Branch**: `011-m7-driver-e2e` | **Date**: 2026-05-11 | **Spec**: [spec.md](./spec.md)
**Input**: Feature specification from `/specs/011-m7-driver-e2e/spec.md`

## Summary

M7 delivers the *demonstration moment* of the compiler track: the
first end-to-end NSL → Verilog pipeline running against the seven
audited open-source NSL projects (`cpu16`, `mips32_single_cycle`,
`ahb_lite_nsl`, `mmcspi`, `SDRAM_Controler`, `rv32x_dev`,
`turboV`). Four orthogonal sub-deliverables converge on a single
acceptance gate:

1. **`nsl-driver` end-to-end wiring (M-track library layer 9 final
   close-out).** New `nslc -emit=verilog` flag that chains M6's
   `nsl→CIRCT` conversion to the stock CIRCT post-processing
   pipeline (`createConvertFSMToSVPass` → `createLowerSeqToSVPass`
   → `createPrepareForEmissionPass`) and finally to
   `circt::exportVerilog` / `circt::exportSplitVerilog`. The flag
   dispatches by the shape of the `-o` argument per Clarifications
   Q1 → B (directory ⇒ split-file; regular file ⇒ single combined;
   stdout ⇒ single combined). New `Compilation::runCIRCTPasses` +
   `Compilation::emit` bodies; new public header
   `include/nsl/Driver/EmitVerilog.h` mirroring `EmitHW.h`'s shape.
   Library boundaries: `nsl-driver` is already established at M2
   and extended each milestone — M7 adds three new TUs
   (`EmitVerilog.cpp`, `RunCIRCTPasses.cpp`, `EmitVerilog.h`'s
   declarations land in the existing list of per-`-emit=*`
   headers). Plan-time decision: NO library rename; the existing
   `nsl-driver` continues as-is (FR-007 plan-time deferred
   resolution: keep the established name; the `add_nsl_library`
   target identity matches the README §Roadmap "`nsl-driver` (9)"
   row already, as confirmed by `lib/Driver/CMakeLists.txt:18`).
2. **P-VEN (vendoring).** Seven audited NSL projects copied
   verbatim under `test/audited/<project>/` with a per-project
   `PROVENANCE.md` recording upstream URL, commit SHA, license,
   vendoring date, and notes. No git submodules; no `FetchContent`;
   no `ExternalProject`. A `cmake/AuditedCorpusLint.cmake` module
   enforces FR-013's structural checks at configure time.
3. **P-VCD (golden VCDs).** Per-project `golden/<scenario>.vcd`
   files sourced from an external known-good source (upstream NSL
   toolchain output for non-CPU projects; manually-authored or
   formal-validated reference for CPU projects). Each golden ships
   with `REGEN.md` documenting the regeneration recipe; a CI lint
   asserts no `REGEN.md` invokes `nslc` (no self-referential
   goldens — Constitution Principle VI "Reference VCDs"
   sub-bullet).
4. **Audited-corpus regression.** New CMake target `check-audited`
   integrated into `ninja check`. For each of the seven projects ×
   each of two simulators (Icarus Verilog + Verilator) = 14 cells,
   the regression: (a) invokes `nslc -emit=verilog` over the
   vendored NSL sources to populate a per-project
   `build/test/audited/<project>/verilog/` directory; (b) compiles
   the emitted Verilog plus the project's testbench under the
   simulator; (c) runs the resulting binary to produce a VCD; (d)
   compares against `golden/<scenario>.vcd` via the vendored
   `tools/vcd_diff.py` (Clarifications Q2 → B: stdlib-only Python
   3.11+ semantic-equal comparator that ignores
   `$date`/`$version`/`$timescale`/`$comment`, intersects signal
   sets via the matched signals plus optional per-project
   `SIGNAL_MAP.toml`, and compares value-change-record sequences).
   Per Clarifications Q3 → A, the dev container is extended with
   Verilator + `riscv-tests` binaries via the established
   `PARENT_IMAGE` build-arg pattern; the new image is tagged
   `ghcr.io/koyamanX/nsl-nslc:dev-m7` (plan-time pin policy:
   non-rolling `:dev-m7` tag specifically so the M7 PR's CI is
   reproducibly bisectable; once M7 merges, the `:dev` rolling tag
   is bumped to match, but the M7 PR does NOT mutate the `:dev`
   tag — that's a follow-on PR).

The technical approach (see [research.md](./research.md) for
Decision/Rationale/Alternatives per choice): MLIR `PassManager`-driven
runtime composition of the three stock CIRCT prep passes (linked via
`CIRCTSVTransforms` and `CIRCTSeqTransforms` link libs that M7 adds
to `nsl-driver`'s `LINK_LIBS`; FSM-to-SV provided by `CIRCTFSMTransforms`
already on the CIRCT side), followed by a direct C++ API call to
`circt::exportVerilog(...)` or `circt::exportSplitVerilog(...)`
chosen by `-o`-argument-shape inspection in `Compilation::emit`.
Diagnostics from anywhere in the post-M6 pipeline (pass-manager
failures, ExportVerilog rejections) route through the project's
single `basic::DiagnosticEngine` via the same `DiagnosticBridge`
RAII handler M5 introduced. The audited-corpus regression is hosted
in lit (`test/audited/lit.cfg.py` extending `test/lit.cfg.py`),
per-project per-simulator substitutions injected via a small
`test/audited/audited_corpus.cmake` helper that auto-discovers
projects by directory presence (SC-006 — adding a new audited
project post-M7 is a routine vendoring-only PR with no infra
edits).

## Technical Context

**Language/Version**: C++17 across `nsl-driver` (Constitution
"Build, Code, and Licensing Standards"). Python 3.11+ for the
vendored `tools/vcd_diff.py` helper (stdlib only; no PyPI deps —
Clarifications Q2 → B). Bash 4+ for lit-test glue and any helper
scripts.

**Primary Dependencies**: LLVM 18 + MLIR 18 + CIRCT (matched to
the `ghcr.io/koyamanX/nsl-nslc:dev-m7` container's pinned versions
— bump from `:dev` per Q3 → A). M7 activates additional CIRCT
libs in `nsl-driver`'s `LINK_LIBS`: `CIRCTExportVerilog` (the
`circt::exportVerilog` + `circt::exportSplitVerilog` entry points),
`CIRCTSeqTransforms` (`createLowerSeqToSVPass`),
`CIRCTSVTransforms` (`createPrepareForEmissionPass`),
`CIRCTFSMTransforms` (`createConvertFSMToSVPass`). Verilator
pinned to `v5.024` (latest stable as of 2026-Q2). `riscv-tests`
pinned to upstream master SHA recorded in the publish-images
lockfile.

**Storage**: N/A (compiler frontend; no persistent runtime state).
Filesystem outputs only: `nslc -emit=verilog` writes Verilog bytes
to `-o <file>` or `-o <dir>/` or stdout; the regression target
writes VCD bytes under `build/test/audited/<project>/<simulator>/`.

**Testing**: lit + FileCheck for the unit-level driver fixtures
under `test/Driver/emit_verilog/` (~10 fixtures: one minimal
single-module source proving the keystone path, plus per-stage
acceptance fixtures); lit + custom `audited-corpus.test-rule`
shell-script substitution for the integration-level audited-corpus
regression under `test/audited/<project>/<scenario>.lit`. gtest
only for unit-level helpers (the `Compilation::emit` argument-shape
dispatch decision table; the `vcd_diff.py` helper has its own
Python `unittest`-based test under `tools/test_vcd_diff.py`).

**Target Platform**: Linux x86_64 (Constitution Principle IX build
matrix); other platforms forward-looking. Dev container is canonical
(`ghcr.io/koyamanX/nsl-nslc:dev-m7`). The audited-corpus regression
requires `iverilog`, `verilator`, `python3` (>=3.11), and (for
`rv32x_dev` + `turboV`) `riscv32-unknown-elf-gcc` + `riscv-tests`
binaries — all bundled by the M7 container.

**Project Type**: Compiler library + driver (single project,
LLVM-style layered architecture per Constitution Principle II). M7
extends the `nsl-driver` library — no new library, no new umbrella
header beyond the one new per-`-emit=*` header (`EmitVerilog.h`)
mirroring the M6 `EmitHW.h` convention already in
`include/nsl/Driver/`. The audited-corpus regression adds a NEW
test top-level (`test/audited/`) sibling to `test/Driver/`,
`test/Lower/`, etc.

**Performance Goals**:
1. M7 lit + audited-corpus regression total wall-clock budget ≤ 15
   minutes inside the dev container on a standard CI runner (SC-005;
   Constitution Principle IX stage 4 timing budget extended for the
   audited cell). Per-project parallelism via lit handles per-cell
   distribution; the slowest individual project is expected to be
   `rv32x_dev` (RISC-V testbench compilation + RISC-V test runner +
   per-instruction trace capture) at ~4–6 minutes.
2. `nslc -emit=verilog` wall-clock on a representative audited
   project (`cpu16` ≈ 2.5k lines NSL) target ≤ 5 seconds on the
   dev container's typical 8-vCPU runner (informational, NOT a CI
   gate).

**Constraints**:
- **Determinism (Principle V)** — every code path producing a name
  MUST use stable iteration; the CI grep introduced at M5 (no
  `std::unordered_*` / pointer-derived ordering / time sources in
  `lib/Lower/`) extends to all M7 code paths under `lib/Driver/`.
  `circt::exportVerilog` itself is byte-deterministic across runs
  on identical input (verified upstream by CIRCT's own
  ExportVerilog roundtrip tests); the M7 contract pins this via a
  two-host-path `diff -q` FileCheck case extending the M5/M6
  pattern. The audited-corpus regression's VCD comparison is
  *semantically* deterministic (Q2 → B) but the underlying VCD
  bytes may differ across simulator versions — `vcd_diff.py`
  isolates that drift.
- **Single public umbrella header per library (Principle II)** —
  `nsl-driver` follows the per-`-emit=*` header convention already
  established at M1/M2/M5/M6 (`EmitTokens.h`, `EmitAST.h`,
  `EmitMLIR.h`, `EmitHW.h`, plus `Sema.h` + `Compilation.h`). M7
  adds exactly one new public header (`EmitVerilog.h`); the
  count-frozen list grows from 7 to 8 (counts include `Version.h.in`'s
  generated `Version.h`).
- **Diagnostic plumbing (Principle IV)** — every `mlir::emitError`
  from CIRCT-side pass passes flows through the `DiagnosticBridge`
  RAII handler M5 introduced. ExportVerilog's own
  diagnostic-bearing failures (e.g., illegal module name) ALSO
  route through this bridge — the M7 implementation registers a
  `mlir::ScopedDiagnosticHandler` for the duration of the
  `Compilation::runCIRCTPasses` + `Compilation::emit` invocations.
- **Stock CIRCT below the `nsl` dialect (Principle III)** — M7
  introduces ZERO new CIRCT-equivalent passes. Every pass in the
  M7 pipeline is a real `circt::*` pass instantiated via its
  upstream `createFooPass` factory.
- **External vendoring (Principle V "deterministic build
  environment")** — P-VEN forbids submodules and `FetchContent`;
  P-VCD forbids self-referential goldens. Both are enforced by CI
  lint at configure time (`cmake/AuditedCorpusLint.cmake`) and at
  build time (`check-audited-lint` target).

**Scale/Scope**:
- **Public-header surface**: 1 new public header (`EmitVerilog.h`),
  growing `nsl-driver`'s public-header count from 7 to 8. No
  changes to other libraries' surfaces.
- **`nsl-driver` source TUs**: 2 new TUs (`EmitVerilog.cpp`,
  `RunCIRCTPasses.cpp`) added to the existing 9-file source list;
  also the `tools/nslc/main.cpp` stub at line 84 is replaced (≤ 6
  new lines of dispatch glue).
- **`Compilation` member functions**: 2 new bodies
  (`runCIRCTPasses` + `emit`); the signatures are already declared
  per `docs/design/nsl_compiler_design.md` §11 lines 1352–1353.
- **CIRCT link libs added to `nsl-driver`**: 4
  (`CIRCTExportVerilog`, `CIRCTSeqTransforms`, `CIRCTSVTransforms`,
  `CIRCTFSMTransforms`). The five dialect libs (`CIRCTHW`,
  `CIRCTComb`, `CIRCTSeq`, `CIRCTSV`, `CIRCTFSM`) are inherited
  transitively from `nsl-lower` (M6).
- **P-VEN vendored sources**: ~12,000 lines of NSL across seven
  projects (matches the `docs/CLAUDE.md` §10 historical estimate;
  per-project breakdown captured in each `PROVENANCE.md`).
- **P-VCD golden VCDs**: estimated 14–25 VCDs total (one or more
  scenarios per project; CPU projects ship more — `rv32x_dev` and
  `turboV` each carry one per instruction-family test).
- **Test fixtures**: ~10 driver lit fixtures
  (`test/Driver/emit_verilog/`) + ~14 audited-corpus lit fixtures
  (`test/audited/<project>/<scenario>.lit` — one per simulator
  per project per scenario; lit's per-substitution generation
  expands the cross-product) + ~5 `cmake/AuditedCorpusLint.cmake`
  configure-time assertions + ~6 Python unit-tests for
  `vcd_diff.py`.
- **New `tools/` entries**: `tools/vcd_diff.py` (~150 LOC stdlib
  Python).

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-check after Phase 1
design.*

### Phase 0 (pre-research) gate

| Principle | Status | Notes |
|---|---|---|
| I. Spec Is Authoritative | **Pass** | M7 introduces no new `Sn`/`Nn`/`Pn` (Forward Roll-up §1 in root `CLAUDE.md` confirms no grammar-track entry adds a column at M7 — M7 is end-to-end driver-completion + corpus). The "no silent AST drops" sub-clause does not apply at this layer (M7 consumes M6's CIRCT IR; it does not parse). |
| II. Layered Library Architecture | **Pass** | M7 extends `nsl-driver` (layer 9); depends on `nsl-lower` (M6) + every M-track library below. No new sibling deps within the M-track. The four NEW CIRCT link libs (`CIRCTExportVerilog`, `CIRCTSeqTransforms`, `CIRCTSVTransforms`, `CIRCTFSMTransforms`) are vendored-upstream-CIRCT libs, NOT new internal `nsl-*` libs. The single-public-umbrella-header rule is not invoked here because `nsl-driver` is already on the established per-`-emit=*`-header convention (NOT one of the named exceptions for `nsl-ast` / `nsl-sema`); EmitVerilog.h slots into that pattern uniformly. |
| III. Stock CIRCT Below | **Pass** | The seam goes live end-to-end. ZERO hand-rolled CIRCT-equivalent passes — all three post-M6 passes (`createConvertFSMToSVPass`, `createLowerSeqToSVPass`, `createPrepareForEmissionPass`) are real upstream-CIRCT passes invoked via `mlir::PassManager::addPass(...)`. ExportVerilog is upstream-CIRCT. |
| IV. Source-Locating Diagnostics | **Pass** | FR-004 routes ALL diagnostics through the project's single `basic::DiagnosticEngine` via the same `DiagnosticBridge` RAII handler M5 introduced. The bridge is scoped to cover the new `runCIRCTPasses` + `emit` member functions. `mlir::Location` continuity from `nsl::*` op → CIRCT op was established at M6; M7 preserves it through the stock-CIRCT passes (which themselves preserve location attrs per upstream's `MLIRContext` invariants). |
| V. Inspectable, Deterministic Pipeline | **Pass** | New `-emit=verilog` flag completes the `-emit=*` staircase (Tokens → AST → NSLMLIR → HW → Verilog), giving every stage an inspectable cut. FR-005 + SC-002 + FR-024 mandate byte-identical output across two builds — the M5/M6 two-host-path `diff -q` determinism gate extends to `-emit=verilog`. The vendored Verilator + `riscv-tests` SHA pins keep the audited regression cell reproducible per Principle V's "deterministic build environment" clause. |
| VI. Layered Test Discipline | **Pass** | This IS the milestone where the end-to-end clause (NON-NEGOTIABLE) materializes. The audited-corpus regression (Story 4 → FR-018 / FR-019) embodies it. P-VEN + P-VCD ("Delivery" + "Reference VCDs" sub-bullets) both land in scope. Self-referential goldens are blocked by FR-016's CI lint. The TWO-simulator requirement (Icarus + Verilator) materializes the sub-bullet's "independent simulator confirmation" expectation. |
| VII. Spec ↔ Design Coupling | **Pass** | No `Sn`/`Nn`/`Pn` change at M7 → no quick-map / line-range update needed in `docs/CLAUDE.md` §5 quick-map. The Forward Roll-up in root `CLAUDE.md` §1 needs no edit (M7 entries are already inferred by README §Roadmap M7 row; the table's "Lower to CIRCT" column is already populated by M6 and M7 does not lower further within `nsl::*` — it just completes the CIRCT-side post-processing). The "Active feature" SPECKIT pointer in root `CLAUDE.md` is updated to point here (Phase 1 step 3). Docs §6 line ranges for `nsl_compiler_design.md` §11 are unchanged (M7 supplies bodies for existing signatures). |
| VIII. Test-First Development | **Pass** | Every new TU lands with its lit fixture authored first (TDD). The `vcd_diff.py` helper lands with its `unittest` suite authored first. The audited-corpus regression's 14 cells each have their per-cell lit fixture authored before `nsl-driver` `-emit=verilog` is operational on that project — early cells initially XFAIL (recorded explicitly), turn green as the pipeline matures. The constructive-`Sn` carve-out from v1.6.0 does not apply at M7 (M7 introduces no `Sn`). |
| IX. Continuous Integration & Delivery | **Pass** | The M0 CI matrix absorbs M7 with one addition: a new lit cell binding `check-audited`. The PARENT_IMAGE bump to `:dev-m7` lands as a self-contained leading commit on the M7 PR per Q3 → A (the publish-images workflow's PARENT_IMAGE pattern is the established mechanism). The M7 PR does NOT bump the rolling `:dev` tag — a follow-on PR does so after M7 merges, isolating the corpus-regression cell to the `:dev-m7` tag during M7's review cycle. No `--no-verify` / no `--no-gpg-sign`. Release artefact provenance (M9 territory) is out of scope. |

### Phase 1 (post-design) gate

Re-evaluated after Phase 1 artefacts (research.md, data-model.md,
contracts/, quickstart.md) are written. The post-design status is
identical to Phase 0 above — no new violations surface during
artefact authoring. All decisions (Q1–Q3 from `## Clarifications`,
plus the planning-time decisions listed in
[`research.md`](./research.md) §§1–9) preserve the constitutional
invariants. The single design choice that warrants explicit
defence — the four new CIRCT `LINK_LIBS` entries
(`CIRCTExportVerilog`, `CIRCTSeqTransforms`, `CIRCTSVTransforms`,
`CIRCTFSMTransforms`) — is forced by the stock-CIRCT pass
pipeline named in design §10 lines 1297–1302 + Q1 → B's
`exportVerilog`/`exportSplitVerilog` runtime dispatch. Each library
is non-substitutable and listed in CIRCT's `CIRCTConfig.cmake`
exports; no Constitution Principle III concern (these ARE the
stock CIRCT entries Principle III mandates).

## Project Structure

### Documentation (this feature)

```text
specs/011-m7-driver-e2e/
├── plan.md                    # This file (/speckit-plan output)
├── spec.md                    # /speckit-specify + /speckit-clarify output
├── research.md                # Phase 0 output (this command)
├── data-model.md              # Phase 1 output (this command)
├── quickstart.md              # Phase 1 output (this command)
├── contracts/                 # Phase 1 output (this command)
│   ├── driver-emit-verilog.contract.md   # CLI flag freeze + dispatch table
│   ├── circt-passes.contract.md          # stock-CIRCT pass pipeline freeze
│   ├── audited-corpus.contract.md        # P-VEN + P-VCD + regression freeze
│   ├── vcd-diff.contract.md              # vcd_diff.py CLI + semantic-equal policy
│   └── container-m7.contract.md          # :dev-m7 image surface freeze
├── checklists/
│   └── requirements.md        # /speckit-specify output (closed by /speckit-clarify)
└── tasks.md                   # Phase 2 output (/speckit-tasks — NOT created here)
```

### Source Code (repository root)

```text
include/nsl/Driver/
├── Compilation.h              # Existing — M7 extends with EmitKind::Verilog
│                              #   dispatch (no signature change; signatures
│                              #   for runCIRCTPasses + emit are already
│                              #   declared per nsl_compiler_design.md §11).
├── EmitTokens.h               # M1 — unchanged at M7.
├── EmitAST.h                  # M2 — unchanged.
├── EmitMLIR.h                 # M5 — unchanged.
├── EmitHW.h                   # M6 — unchanged.
├── EmitVerilog.h              # NEW M7 — public entry for `-emit=verilog`,
│                              #   mirrors EmitHW.h shape. Sole new symbol:
│                              #   `nsl::driver::emitVerilog(...)`.
├── Sema.h                     # M3 — unchanged.
└── Version.h.in               # M0 — unchanged.

lib/Driver/
├── CMakeLists.txt             # M7 — extend source list with EmitVerilog.cpp
│                              #   and RunCIRCTPasses.cpp; extend HEADERS with
│                              #   EmitVerilog.h; extend LINK_LIBS with the
│                              #   four new CIRCT libs (CIRCTExportVerilog +
│                              #   CIRCTSeqTransforms + CIRCTSVTransforms +
│                              #   CIRCTFSMTransforms).
├── Compilation.cpp            # M5/M6 — adds runCIRCTPasses() + emit() bodies.
├── EmitTokens.cpp             # M1 — unchanged.
├── EmitAST.cpp                # M2 — unchanged.
├── EmitMLIR.cpp               # M5 — unchanged.
├── EmitHW.cpp                 # M6 — unchanged.
├── EmitVerilog.cpp            # NEW M7 — emitVerilog() driver glue, ~140 LOC
│                              #   (mirrors EmitHW.cpp shape with two
│                              #    additional stages: runCIRCTPasses then
│                              #    emit). Dispatches by -o argument shape.
├── LowerToCIRCT.cpp           # M6 — unchanged.
├── LowerToNSL.cpp             # M5 — unchanged.
├── RunCIRCTPasses.cpp         # NEW M7 — builds + runs the mlir::PassManager
│                              #   that schedules createConvertFSMToSVPass +
│                              #   createLowerSeqToSVPass +
│                              #   createPrepareForEmissionPass. ~80 LOC.
├── RunNSLPasses.cpp           # M5 — unchanged.
└── Sema.cpp                   # M3 — unchanged.

tools/nslc/
└── main.cpp                   # M7 — replace line-84 stub with a call into
                               #   nsl::driver::emitVerilog. ~6 LOC delta.

tools/
└── vcd_diff.py                # NEW M7 — Python 3.11+ stdlib semantic-equal
                               #   VCD comparator. ~150 LOC.
                               #   Companion test: test_vcd_diff.py (~60 LOC,
                               #   Python unittest).

test/Driver/
└── emit_verilog/              # NEW M7 — driver-unit fixtures.
    ├── single_module.test     # ~1 register + 1 wire; FileCheck pins module
    │                          #   header + reg always_ff + wire assign.
    ├── multi_module_dir.test  # split-file dispatch (Q1 → B path a).
    ├── multi_module_file.test # single-file dispatch (Q1 → B path b).
    ├── multi_module_stdout.test  # stdout dispatch (Q1 → B path c).
    ├── determinism.test       # two-build diff -q (FR-024 / SC-002).
    ├── sema_error.test        # negative path: Sema error blocks emit (FR-004).
    ├── passes_failure.test    # negative path: bogus IR survives lowerToCIRCT
    │                          #   but trips runCIRCTPasses → diagnostic.
    ├── export_failure.test    # negative path: trips ExportVerilog → diagnostic.
    ├── iverilog_smoke.test    # post-emit, run `iverilog -g2012` on output —
    │                          #   asserts syntactic acceptance.
    └── verilator_smoke.test   # post-emit, run `verilator --lint-only` —
                               #   asserts syntactic acceptance.

test/audited/                  # NEW M7 — vendored corpus + regression.
├── lit.cfg.py                 # Extends test/lit.cfg.py; adds
│                              #   `%nsl-driver-verilog` substitution,
│                              #   `%audited-simulate` substitution per
│                              #   simulator, `%vcd-diff` substitution.
├── audited_corpus.cmake       # CMake glue: enumerates project dirs,
│                              #   generates per-project per-simulator
│                              #   lit-fixture instances, registers
│                              #   `check-audited` target.
├── cpu16/                     # Vendored upstream.
│   ├── PROVENANCE.md
│   ├── *.nsl                  # NSL sources (verbatim copy).
│   ├── tb/                    # Testbench(es) — verbatim copy if upstream
│   │                          #   ships; manually-authored if not (rare
│   │                          #   for non-CPU projects).
│   └── golden/
│       ├── REGEN.md
│       └── <scenario>.vcd
├── mips32_single_cycle/       # (same shape as cpu16)
├── ahb_lite_nsl/
├── mmcspi/
├── SDRAM_Controler/
├── rv32x_dev/                 # CPU project — testbench typically
│   ├── tb/                    #   instruction-stream-driven; golden VCDs
│   ├── tests/                 #   manually-authored OR riscv-formal export
│   │                          #   (M8 territory) — per Q4-implicit + FR-017.
│   └── golden/
│       ├── REGEN.md
│       ├── add.vcd            # Per-instruction-family scenarios.
│       ├── load.vcd
│       └── …
└── turboV/                    # (same shape as rv32x_dev)

cmake/
└── AuditedCorpusLint.cmake    # NEW M7 — configure-time assertions
                               #   (FR-013): (a) seven project dirs exist,
                               #   (b) each has PROVENANCE.md with the
                               #   required Key: lines, (c) no submodule
                               #   reference, (d) no FetchContent/ExternalProject
                               #   path through test/audited/, (e) no
                               #   golden/REGEN.md invokes `nslc`.

docker/                        # M7 — extend.
├── Dockerfile.dev             # Existing — bump to add verilator (v5.024)
│                              #   + riscv-tests binaries.
└── publish-images.yml         # GitHub Actions workflow — bump to publish
                               #   `:dev-m7` tag using PARENT_IMAGE pattern.

CLAUDE.md                      # Root — M7 updates the §SPECKIT "Active
                               #   feature" block to point at this plan.
                               #   No §1/§2 roll-up edit needed (README
                               #   §Roadmap M7 row carries the entry).
```

**Structure Decision**: Single project, LLVM-style layered
architecture per Constitution Principle II. The `nsl-driver` library
established at M2 is extended in place — M7 adds 2 new TUs
(`EmitVerilog.cpp`, `RunCIRCTPasses.cpp`), 1 new public header
(`EmitVerilog.h`), 4 new CIRCT link libs in `LINK_LIBS`. No new
internal library, no new public umbrella header beyond the
established per-`-emit=*`-header pattern. The audited-corpus
regression adds a NEW top-level under `test/audited/` and a single
new CMake helper (`cmake/AuditedCorpusLint.cmake`). The new
`tools/vcd_diff.py` is the project's first non-C++ tool committed
to the tree — a stdlib-only Python script with no PyPI deps per
Q2 → B; its presence does NOT establish a Python-tooling
sub-architecture (the project remains C++ + CMake + lit). Total
new code: ~140 + 80 LOC in `lib/Driver/` + ~150 LOC in
`tools/vcd_diff.py` + ~6 LOC in `tools/nslc/main.cpp` + ~250 LOC
CMake (audited-corpus.cmake + AuditedCorpusLint.cmake + CMakeLists
edits) ≈ ~625 LOC. Vendored corpus is ~12,000 lines of NSL (not
counted toward "new code"). Test fixture growth: ~10 driver lit
fixtures + ~25 audited-corpus lit fixtures (auto-generated by
`audited_corpus.cmake`) + ~6 Python unittests.

## Complexity Tracking

> **No Constitution Check violations**: Phase 0 and Phase 1 gates
> both pass without exception. Complexity tracking table omitted.

Three design choices warrant explicit defence (each annotated in
[research.md](./research.md) with Decision/Rationale/Alternatives):

1. **Four new CIRCT `LINK_LIBS` entries** (`CIRCTExportVerilog`,
   `CIRCTSeqTransforms`, `CIRCTSVTransforms`,
   `CIRCTFSMTransforms`). Non-substitutable — each provides one
   `circt::*` entry point named in design §10 lines 1297–1302
   without which the stock-CIRCT pass pipeline cannot be assembled.
   Adding all four at the same M7 point ensures no half-shipped
   driver state (e.g., FSM-to-SV converted but seq-to-SV missing).
2. **Two-simulator regression matrix** (Icarus + Verilator) rather
   than one. Forced by Constitution Principle VI's "Reference VCDs"
   sub-bullet expectation of independent simulator confirmation;
   the Q3 → A container expansion bakes this in. Discussed in
   [research.md](./research.md) §6.
3. **Python 3.11+ for `vcd_diff.py`** rather than C++. The
   comparison logic is naturally ~150 LOC of dict / list
   manipulation; a C++ implementation would be ~500 LOC with no
   functional gain. Python is already in the dev container (CMake
   itself depends on Python 3 for several macros). No PyPI deps
   means no transitive-dep surface to audit. Discussed in
   [research.md](./research.md) §4.

Phase 0 gate evaluated 2026-05-11 → Pass.
Phase 1 gate re-evaluated 2026-05-11 (post-research.md /
data-model.md / contracts/ authoring) → Pass (no new violations
surfaced).
