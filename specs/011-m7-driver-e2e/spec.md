<!-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception -->

# Feature Specification: M7 — `nsl-driver` end-to-end (`nslc -emit=verilog`); P-VEN vendoring + P-VCD golden VCDs + audited-corpus regression

**Feature Branch**: `011-m7-driver-e2e`
**Created**: 2026-05-11
**Status**: Draft
**Input**: User description: "M7 — end-to-end NSL → Verilog driver. Wire `nslc -emit=verilog input.nsl` to consume M6's nsl→CIRCT IR and produce a byte-stable Verilog file via stock CIRCT passes (`--convert-fsm-to-sv`, `--lower-seq-to-sv`, any other required prep) followed by `circt::ExportVerilog`. Two M7-gating deliverables also land in scope: P-VEN — vendor all four audited projects (cpu16, mips32_single_cycle, ahb_lite_nsl, mmcspi, SDRAM_Controler, rv32x_dev, turboV) under `test/audited/<project>/` with `PROVENANCE.md` (URL + SHA + license per project; no git submodules, no configure-time fetches); and P-VCD — author `test/audited/<project>/golden/<scenario>.vcd` golden VCDs sourced externally (upstream NSL toolchain output for non-CPU projects; manually-authored/formal-validated for CPU projects), each with `REGEN.md`. The acceptance criterion (Principle VI NON-NEGOTIABLE): all four audited projects compile via `nslc -emit=verilog` and simulate equivalently to their `golden/*.vcd` under both Icarus Verilog and Verilator. Principle V determinism: two builds with identical inputs/flags produce byte-identical Verilog output (verify via FileCheck case). Principle IX: no `--no-verify` bypass; all artifacts CI-built."

> **Scope interpretation.** "M7" maps to the **M7** row of
> [`README.md`](../../README.md) §Roadmap, which delivers the
> *demonstration moment* of the compiler track: the first
> NSL-source-to-Verilog-bytes pipeline running against the
> audited open-source NSL projects (the 4-project corpus at
> M7 — see FR-009 amendment 2026-05-12 for the corpus
> narrowing). The deliverable surface is three orthogonal
> parts that all land on or before M7 per the README's
> "Required Deliverables" table:
>
> 1. **`nsl-driver` (layer 9)** — wires the M6-output CIRCT IR to
>    Verilog bytes. New driver flag: `nslc -emit=verilog`. New
>    member function `Compilation::runCIRCTPasses` (the stock-CIRCT
>    pass pipeline named in
>    [`docs/design/nsl_compiler_design.md`](../../docs/design/nsl_compiler_design.md)
>    §10 lines 1297–1302), and `Compilation::emit` (the
>    `circt::exportVerilog` invocation per §10 line 1302). Both
>    member-function signatures are already declared in
>    `docs/design/nsl_compiler_design.md` §11 lines 1352–1353; M7
>    supplies the bodies. Public surface added to
>    `include/nsl/Driver/`: `EmitVerilog.h` (1 header) mirroring the
>    M6 `EmitHW.h` convention.
> 2. **P-VEN (vendoring)** — four audited projects
>    (`cpu16`, `mips32_single_cycle`, `ahb_lite_nsl`, `turboV`)
>    vendored under `test/audited/<project>/` per the
>    Constitution Principle VI "Delivery" sub-bullet. Corpus
>    narrowed from originally-projected 7 per FR-009 amendment
>    (2026-05-12; license audit). Each project carries
>    `PROVENANCE.md` recording upstream URL, commit SHA at time of
>    vendoring, and license (must be Apache-2.0-WITH-LLVM-exception
>    compatible). **No git submodules. No configure-time fetches.**
> 3. **P-VCD (golden VCDs)** — `test/audited/<project>/golden/<scenario>.vcd`
>    files sourced from an external known-good source per the
>    Constitution Principle VI "Reference VCDs" sub-bullet — upstream
>    NSL toolchain output for non-CPU projects, manually-authored or
>    formal-validated reference for CPU projects. Each golden ships
>    with `golden/REGEN.md` documenting the regeneration recipe.
>    **Self-referential VCDs (regenerated from `nslc`'s own emitted
>    Verilog at test time) are NOT acceptable** — they only catch
>    `nslc`-vs-previous-`nslc` regressions, not NSL-correctness.
>
> Plus one acceptance gate that joins all three:
>
> 4. **Audited-corpus regression** — a lit test layer that, for each
>    of the four projects, runs `nslc -emit=verilog` over the
>    project's NSL sources, simulates the result against the
>    project's testbench under **both** Icarus Verilog and Verilator,
>    and asserts equivalence against the project's `golden/*.vcd`.
>    The Constitution Principle VI end-to-end clause is
>    NON-NEGOTIABLE: a change that breaks any of these
>    regressions does not land.
>
> **What does NOT land at M7.** No riscv-formal integration — that
> is M8. No release-pipeline plumbing (tagged binaries, reproducible
> source tarball) — that is M9. No new `nsl::*` ops, no new CIRCT
> conversion patterns, no new structural passes — M7 is strictly
> the driver-completion + corpus-arrival milestone. M7 also does not
> modify the M6 conversion pass output: the `nsl::*` → CIRCT
> boundary is M6's; the CIRCT → Verilog tail is M7's. The Tooling
> track (T-track) is independent of M7 — T2/T3/T8 work proceeds in
> parallel.
>
> **Stock CIRCT pass identity.** The vendored CIRCT in
> `ghcr.io/koyamanX/nsl-nslc:dev` ships `--convert-fsm-to-sv` as
> the pass-flag name; the design doc names the C++-API entry as
> `circt::fsm::convertFSMToSeq`. These are the **same conversion**
> (FSM → state register + next-state combinational logic). M7
> invokes the API entries directly via
> `mlir::PassManager::addPass(circt::fsm::createConvertFSMToSVPass())`
> et al.; FileCheck reference fixtures use the C++-API-correspondent
> pass-name string `--convert-fsm-to-sv` since that is what the
> vendored container's `circt-opt` accepts. Naming drift between
> upstream-canonical (`convertFSMToSeq`) and vendored-shipped
> (`convertFSMToSV`) is captured in the M6 retrospective
> commentary; M7 commits to the vendored container's reality
> (Principle V — same toolchain, same bytes).

## Clarifications

### Session 2026-05-11

- Q: When `nslc -emit=verilog input.nsl` is invoked on a source that defines multiple top-level modules, how is the output structured — one combined `.v` file written to `-o <file>` (or stdout), or one `.v` per module written to `-o <dir>/`? → A: **Option B — hybrid runtime resolution.** When `-o <path>` ends in `/` or names an existing directory, invoke `circt::exportSplitVerilog` and emit one `.v` per `hw.module` into that directory (file naming per CIRCT's default: `<hw.module-name>.v` for non-`extern` modules; `<top>.sv` for the dual-suffix top per CIRCT's convention). Otherwise (`-o <regular-file>` or `-o -` or `-o` omitted) invoke `circt::exportVerilog` and emit one combined `.v` to that file or stdout. The audited-corpus regression's lit fixture uses directory mode (so per-module testbenches can `include` or compile-list individual `.v` files); FileCheck fixtures under `test/Driver/emit_verilog/` use stdout mode (so a single `// CHECK:` stream can verify the entire output). Rationale: keeps both surfaces ergonomic without forcing a CLI flag on every audited-corpus invocation and without forking testbench infrastructure. Option A (single-file always) was rejected because audited testbenches expect per-module filenames matching their `golden/<module>.vcd` naming; Option C (split-file always) was rejected because it makes FileCheck fixtures awkward (`cat dir/*.v | FileCheck …` loses module-boundary information); Option D (explicit `--split-verilog` flag) was rejected because it adds CLI surface that every audited-corpus test would have to remember.
- Q: What is the equivalence criterion for the audited-corpus regression — strict byte-equal VCD comparison vs `vcddiff`-style semantic-equal (tolerating timestamp-header drift and internal-signal-naming differences)? → A: **Option B — semantic-equal via vendored Python helper `tools/vcd_diff.py`.** The helper is implemented against the Python standard library only (no third-party PyPI deps; the dev container ships Python 3.11+). Comparison policy: (a) ignore `$date`, `$version`, `$timescale`, `$comment` declarations entirely (these are simulator-toolchain metadata, not RTL semantics); (b) extract the `$var`-block signal set as `(scope-path, signal-name, width)` tuples; (c) treat a `nslc`-emitted-signal as matching a golden-signal when both signals share an identical signal-bus mnemonic OR are explicitly aliased via an optional per-project `test/audited/<project>/golden/SIGNAL_MAP.toml` (when present); (d) compare value-change records on the matched-signal intersection — equal when, for each simulation timestamp, both sides record the same value sequence on the same matched signal. The helper exits zero on equal, non-zero on diff, and on diff emits a human-readable per-signal first-divergence report to stderr per FR-022. Rationale: golden VCDs were captured against upstream toolchain simulators whose `$date`/`$version`/`$timescale` headers drift from Icarus/Verilator output even on identical RTL — strict-byte-equal would guarantee header-only false-fails. Option A (strict byte-equal) was rejected for that exact reason. Option C (per-cycle reference traces, bypassing VCD entirely) was rejected because it would require authoring per-cycle reference traces for every project — orders of magnitude more authoring than P-VCD already implies. Option D (third-party `vcd-diff` binary) was rejected because it introduces a build-time dependency on an external tool whose equivalence policy the project cannot pin precisely; vendoring its source adds container complexity for no benefit over a 150-line Python helper. `SIGNAL_MAP.toml` is optional — empty (or absent) means upstream and `nslc`-emitted signal names match verbatim, which is the expected default for most projects.
- Q: Where does the audited-corpus simulation host execute? Inside the dev container (currently `ghcr.io/koyamanX/nsl-nslc:dev` per the project memory `project_build_environment.md`), or does M7 require a fresh container image that bundles Icarus Verilog + Verilator + the vendored audited testbenches' transitive deps (e.g. `iverilog` is present, but `verilator` may not be; CPU projects may require `riscv-tests` binaries)? → A: **Option A — extend the dev container.** Add Verilator + per-project simulation deps (e.g. `riscv-tests` binaries for `rv32x_dev` + `turboV`) to the existing `Dockerfile.dev` via the established `PARENT_IMAGE` build-arg pattern (`project_publish_images_buildx_isolation.md`). Land the image bump as a self-contained PR (or a self-contained leading commit on the M7 PR), tagging the new image e.g. `ghcr.io/koyamanX/nsl-nslc:dev-m7` (or bumping the `:dev` rolling tag — pin policy at `/speckit-plan`). CI matrix pins the new tag for the lit cell that runs `check-audited`. Verilator version: pin to a specific upstream tag (`v5.x`); the SHA goes into the publish-images workflow's lockfile. Rationale: Constitution Principle V deterministic-build-environment + Principle VI's "Reference VCDs" sub-bullet make the audited-corpus regression a NON-NEGOTIABLE M7 acceptance gate, and that gate MUST be runnable inside the canonical container. Option B (Icarus only) halves the regression matrix to 7 cells, eroding Principle VI's independent-simulator confirmation. Option C (host-installed simulators) is a hard Principle V violation. Option D (defer Verilator to post-M7) weakens M7's milestone definition; post-M7 Verilator bugs may indicate emit-time defects that should have blocked M7.

## User Scenarios & Testing *(mandatory)*

### User Story 1 — `nslc -emit=verilog` produces byte-stable Verilog for a single representative module (Priority: P1)

A contributor authors (or selects from the M3/M5/M6 corpus) a
representative `.nsl` source file containing one `module` block that
exercises the M6 nsl→CIRCT conversion (e.g. a register, an FSM, a
combinational expression). They run
`nslc -emit=verilog input.nsl -o output.v`, and observe that: (1)
the command exits zero with no stderr; (2) `output.v` is a
syntactically valid Verilog file; (3) the file is byte-identical
across two consecutive invocations with identical inputs and flags;
(4) compiling `output.v` under Icarus Verilog (`iverilog -g2012`)
produces a runnable simulation binary.

**Why this priority**: This is the keystone — every other M7
deliverable (P-VEN, P-VCD, audited-corpus regression) depends on
`-emit=verilog` being operational. It is also the first observable
NSL-source-to-Verilog-bytes pipeline since the project's inception
and the milestone's most-cited acceptance signal in README §Roadmap.

**Independent Test**: Can be fully tested by a single hand-authored
FileCheck fixture under `test/Driver/emit_verilog/` that contains a
minimal `.nsl` source and `// CHECK:` lines pinning representative
output (module declaration line, one `assign`, one `always_ff`),
plus a determinism FileCheck case that runs `nslc -emit=verilog`
twice and `diff`s the outputs. Delivers value: every M6 fixture
file becomes a candidate end-to-end fixture.

**Acceptance Scenarios**:

1. **Given** an NSL source file with one `module` block and one `reg` declaration, **When** `nslc -emit=verilog input.nsl -o output.v` is invoked, **Then** the command exits zero and `output.v` contains a `module` declaration with one `always_ff @(posedge m_clock or posedge p_reset)` block for the register (M6 Q2 → C convention applies — async active-HIGH `p_reset`).
2. **Given** the same NSL source file, **When** `nslc -emit=verilog input.nsl -o run1.v && nslc -emit=verilog input.nsl -o run2.v && diff run1.v run2.v` is run, **Then** the `diff` exits zero (byte-identical output — Principle V).
3. **Given** the emitted `output.v`, **When** `iverilog -g2012 -o output.vvp output.v && vvp output.vvp` is run (Icarus Verilog), **Then** the simulator accepts the Verilog and produces no syntax errors.
4. **Given** an NSL source with a Sema error (e.g. S15-violating non-constant bit-slice), **When** `nslc -emit=verilog input.nsl` is invoked, **Then** the command exits non-zero with a diagnostic from the project's single `basic::DiagnosticEngine` and produces no `output.v` (Principle IV — diagnostics route through one channel; no leaked MLIR/CIRCT diagnostics).

---

### User Story 2 — P-VEN: seven audited NSL projects vendored deterministically (Priority: P2)

A maintainer reviewing the project tree finds
`test/audited/cpu16/`, `test/audited/mips32_single_cycle/`,
`test/audited/ahb_lite_nsl/`, `test/audited/mmcspi/`,
`test/audited/SDRAM_Controler/`, `test/audited/rv32x_dev/`, and
`test/audited/turboV/` directories present. Each contains the
upstream NSL source files copied in verbatim (no submodules, no
fetch scripts) and a `PROVENANCE.md` recording upstream URL, commit
SHA at time of vendoring, vendoring date, and license. The
maintainer can read `PROVENANCE.md` and reconstruct exactly which
upstream commit the vendored sources correspond to.

**Why this priority**: P-VEN is the prerequisite for the
audited-corpus regression (Story 4) — without vendored sources,
there is nothing to feed `nslc -emit=verilog` for end-to-end tests.
It is also a Constitution Principle V (deterministic build
environment) gate: submodules and configure-time fetches make the
build environment irreproducible.

**Independent Test**: Can be fully tested by a CI step that asserts
(a) each of the seven directories exists, (b) each contains
`PROVENANCE.md` with non-empty `URL:`, `SHA:`, `License:`,
`VendoredAt:` fields, (c) no `.gitmodules` entry references
`test/audited/`, (d) no `CMakeLists.txt` under `test/audited/`
invokes `FetchContent_Declare` or `ExternalProject_Add`. Delivers
value: even without `nslc -emit=verilog` working, the corpus is
catalogued and ready for inspection.

**Acceptance Scenarios**:

1. **Given** a fresh clone of the repository, **When** `ls test/audited/` is run, **Then** seven directories are listed: `cpu16`, `mips32_single_cycle`, `ahb_lite_nsl`, `mmcspi`, `SDRAM_Controler`, `rv32x_dev`, `turboV`.
2. **Given** any of the seven `test/audited/<project>/` directories, **When** its `PROVENANCE.md` is read, **Then** the file contains at minimum: (a) `Upstream-URL:` line with a working URL, (b) `Upstream-SHA:` line with a 40-character hex SHA, (c) `License:` line naming a license compatible with Apache-2.0-WITH-LLVM-exception, (d) `Vendored-At:` line with an ISO-8601 date.
3. **Given** the repository tree, **When** `grep -r 'submodule\|FetchContent\|ExternalProject' test/audited/ .gitmodules` is run (where `.gitmodules` may or may not exist), **Then** no match references any of the four audited projects.
4. **Given** the repository, **When** the build is invoked offline (`--no-network` in container terms), **Then** the audited-corpus build succeeds — no network access is required to construct the corpus tree.

---

### User Story 3 — P-VCD: golden VCDs sourced externally with regeneration recipes (Priority: P2)

A maintainer reviewing `test/audited/<project>/golden/` finds one
or more `<scenario>.vcd` files plus a `REGEN.md` documenting the
regeneration recipe. Reading `REGEN.md`, the maintainer can identify
the external source from which the golden was captured (upstream NSL
toolchain version + invocation, or formal-validation framework +
testbench), and the simulator under which the VCD was recorded.
No golden was captured from `nslc`'s own emitted Verilog.

**Why this priority**: P-VCD is the equivalence target for the
audited-corpus regression (Story 4). The Constitution Principle VI
"Reference VCDs" sub-bullet makes the external-source rule
NON-NEGOTIABLE: self-referential goldens only catch
`nslc`-vs-previous-`nslc` regressions, not NSL-correctness.

**Independent Test**: Can be fully tested by inspection — each
golden has its `REGEN.md` peer; `REGEN.md` names a regeneration
command; the regeneration command does not invoke `nslc`. Delivers
value: the goldens become reviewable assets independently of whether
`nslc -emit=verilog` works.

**Acceptance Scenarios**:

1. **Given** any of the seven `test/audited/<project>/golden/` directories, **When** its contents are listed, **Then** at least one `<scenario>.vcd` file is present and a `REGEN.md` peer is present.
2. **Given** `test/audited/<project>/golden/REGEN.md`, **When** the file is read, **Then** it contains: (a) the regeneration command line, (b) the external-source identification (e.g. "Generated by upstream NSL Studio 1.4 commit XXX, simulator: Icarus Verilog 11.0"), (c) any environment/version dependencies needed to reproduce.
3. **Given** the regeneration command in any `REGEN.md`, **When** the command is inspected, **Then** it does NOT invoke `nslc` (no self-referential goldens).
4. **Given** a CPU project (`cpu16`, `mips32_single_cycle`, `rv32x_dev`, `turboV`), **When** the golden's provenance is examined, **Then** `REGEN.md` documents either a manually-authored reference trace (with the testbench source) or a formal-validation framework's output (e.g. riscv-formal trace export).

---

### User Story 4 — Audited-corpus regression: all four projects simulate equivalently under two simulators (Priority: P1)

A CI run (or a contributor running `cmake --build build --target check-audited`) executes the audited-corpus regression: for each of the four projects, `nslc -emit=verilog` produces Verilog from the project's NSL sources; the project's testbench is compiled and simulated against the emitted Verilog under **both** Icarus Verilog and Verilator; the resulting VCD is compared against the project's `golden/*.vcd`; the comparison passes (per the equivalence criterion pinned by Clarifications Q2). All four projects pass under both simulators.

**Why this priority**: This IS the M7 acceptance gate — the
Constitution Principle VI end-to-end clause is NON-NEGOTIABLE and
this regression is its only embodiment. Tied with Story 1 for P1
because Story 1 is the unit-level proof and Story 4 is the
system-level proof, and the milestone is not "M7" until both
pass.

**Independent Test**: Can be fully tested by `cmake --build build --target check-audited` (or `lit test/audited/`). Requires Stories 1, 2, 3 to all be delivered. Delivers value: this is the first NSL-correctness signal the project has ever had; every M-track change post-M7 has this regression as its safety net.

**Acceptance Scenarios**:

1. **Given** the four vendored audited projects and their goldens, **When** `cmake --build build --target check-audited` is run inside the dev container, **Then** each of the four projects produces a Verilog file via `nslc -emit=verilog` that compiles cleanly under both Icarus Verilog and Verilator (zero exit code; no warnings escalated to errors).
2. **Given** the per-project simulator output VCD, **When** the equivalence comparison runs against `golden/*.vcd`, **Then** the comparison passes per Clarifications Q2 (semantic-equal: same `$var` signal set, same value-change-record sequence; `$date`/`$version`/`$timescale` headers ignored).
3. **Given** a deliberate breakage of any M-track library (e.g. revert one M6 conversion pattern), **When** the audited-corpus regression runs, **Then** at least one of the four projects fails — the regression must be *load-bearing*, not trivially-pass.
4. **Given** the regression is integrated into CI, **When** any PR runs through CI, **Then** the audited-corpus regression is part of the merge gate (Constitution Principle IX — no bypass).
5. **Given** the four projects, **When** the regression runs end-to-end, **Then** the total wall-clock time is bounded (target: ≤ 15 minutes inside the dev container on a standard CI runner) — slow regressions get bypassed in practice and erode Principle VI.

---

### Edge Cases

- What happens when an audited project's NSL source uses an NSL feature that has not yet landed (e.g. a hypothetical M8 feature)? → The vendoring step records what is vendored; the regression step skips (with a documented `XFAIL` reason) any scenario whose feature surface exceeds what M6+M7 deliver. Self-correcting: post-M8 PRs unfreeze those XFAILs.
- What happens when one simulator (Icarus or Verilator) reports a Verilog-syntax warning but the other accepts? → The regression treats both as authoritative — both must accept the emitted Verilog without warnings escalated to errors. Per-simulator-only acceptance is not "M7-passing".
- What happens when a golden VCD's signal set is a strict superset of the simulator-emitted VCD (e.g. the upstream-NSL-toolchain golden exposes internal signals that `nslc` optimizes away)? → The semantic-equal comparison treats this as a tolerable difference: the comparison passes if the *intersection* signal set's value-change records match. The non-intersected signals are recorded in `REGEN.md` as "internal-only-on-upstream" so future maintainers know why.
- What happens when an `nslc -emit=verilog` invocation produces Verilog that one simulator accepts and the other rejects with a hard syntax error? → Treated as a `nslc` bug, not a simulator bug: emitted Verilog MUST be acceptable to the canonical SystemVerilog-2012 / IEEE-1800-2009 subset that both simulators agree on. The regression fails the project.
- What happens when a vendored project's license is GPL/copyleft? → P-VEN's license-compatibility check blocks the vendoring before the project lands. Resolution: the project is dropped from the corpus or relicensed by upstream consent (the four projects on the list have already been vetted; this edge case is about future additions).
- What happens when two consecutive `nslc -emit=verilog` invocations produce different bytes? → Treated as a hard Principle V violation: the build fails. Common root causes (and which this spec rules out): `std::unordered_map` iteration order (use deterministic containers); time-based identifiers (forbidden); environment-variable bleed (driver scrubs locale + `LC_*`); LLVM/MLIR `ValueRange` iteration with non-deterministic order (use stable sorts).
- What happens when `-emit=verilog` is invoked with `-o -` (stdout)? → Single-file output to stdout. Useful for piping into `verilator --lint-only` or other quick checks. Per Clarifications Q1 — single-file when `-o -` or `-o <regular-file>`; split-file when `-o <directory>/`.

## Requirements *(mandatory)*

### Functional Requirements

#### Driver wiring (Story 1)

- **FR-001**: The compiler driver MUST accept `-emit=verilog` as a stage flag on `nslc`. This flag corresponds to `CompileOptions::EmitKind::Verilog` (already declared in `docs/design/nsl_compiler_design.md` §11 line 1317; default emit value per line 1318). With this flag set, the pipeline runs preprocess → lex → parse → sema → lowerToNSL → runNSLPasses → lowerToCIRCT → **runCIRCTPasses → emit** (the last two are M7's deliverables); all earlier stages are M1–M6's deliverables and are not modified by M7.
- **FR-002**: `Compilation::runCIRCTPasses` MUST invoke the stock CIRCT pass pipeline pinned by `docs/design/nsl_compiler_design.md` §10 + `specs/011-m7-driver-e2e/contracts/circt-passes.contract.md` §1: (1) `circt::createConvertFSMToSVPass()` (FSM → state-register/next-state lowering; vendored container ships `--convert-fsm-to-sv`); (2) `circt::createLowerSeqToSVPass()`. `PrepareForEmission` is NOT invoked explicitly — it runs internally inside `circt::exportVerilog` per upstream `circt/Conversion/Passes.td:76`, and explicit invocation also fails the PassManager root-op binding check (the pass declares no root-op anchor). After this member function returns success and ExportVerilog runs PrepareForEmission internally, the emitted Verilog contains only `hw`/`comb`/`sv` semantics (no `nsl`, no `fsm`, no `seq` residue). **Amendment 2026-05-12 (FR-002)**: original FR-002 named a 3-slot pipeline in `circt::fsm::` / `circt::seq::` / `circt::sv::` sub-namespaces; the vendored CIRCT exposes 2 of the 3 in the flat `circt::` namespace and runs PrepareForEmission inside ExportVerilog. See `circt-passes.contract.md` §1.1 + `docs/design/nsl_compiler_design.md` §10 retrospective.
- **FR-003**: `Compilation::emit` MUST dispatch on the shape of the `-o` argument per Clarifications Q1 → B: (a) if `-o <path>` ends in `/` or names an existing directory, invoke `circt::exportSplitVerilog(module, path)` and emit one `.v` per `hw.module` into the directory; (b) if `-o <file>` names a regular-file path, invoke `circt::exportVerilog(module, &outputFile)` and emit one combined `.v` to that file; (c) if `-o -` or `-o` omitted entirely, invoke `circt::exportVerilog(module, &llvm::outs())` and emit to stdout. The audited-corpus regression uses path (a); FileCheck fixtures use path (c).
- **FR-004**: Failures inside `runCIRCTPasses` or `emit` (e.g. stock-CIRCT pass verifier failure, ExportVerilog rejection) MUST route through the project's single `basic::DiagnosticEngine` (Constitution Principle IV). CIRCT's internal `mlir::emitError` diagnostics MUST be bridged through a `mlir::ScopedDiagnosticHandler` registered at M7; users see one diagnostic channel, never a leaked MLIR/CIRCT stderr line.
- **FR-005**: Two invocations of `nslc -emit=verilog <input> -o <output>` with identical inputs, flags, environment, and toolchain version MUST produce byte-identical `<output>` files (Constitution Principle V).
- **FR-006**: A new public header `include/nsl/Driver/EmitVerilog.h` MUST be added (mirroring the M6 `EmitHW.h` convention from `tools/nslc/main.cpp` lines 6–9). The header exposes the `nsl::driver::emitVerilog(...)` entry point. Per Constitution Principle II's single-public-header rule **within a library**, this is consistent with M6's pattern (one header per `-emit=*` action under `Driver/`).
- **FR-007**: A new `nsl-driver` static library MUST exist (per the README §Roadmap M7 row designation "`nsl-driver` (9)"). At minimum, the library hosts `Compilation::runCIRCTPasses`, `Compilation::emit`, and the `nsl::driver::emitVerilog` entry. The library is added to the `add_nsl_library` ladder in `lib/Driver/CMakeLists.txt` (the existing `lib/Driver/` directory may either *be* `nsl-driver` once M7 lands or remain `nsl-driver-emit-*` glue — pin one of these in `/speckit-plan`; spec-level only requires that the library identity is named `nsl-driver`).
- **FR-008**: The `tools/nslc/main.cpp` entry point's stub at line 84 (the current `"error: '-emit=verilog' is not yet implemented (planned for M7)"`) MUST be replaced by a call into `nsl::driver::emitVerilog(...)`. Driver `main.cpp` stays ≤ 110 lines total (**2026-05-12 amendment**: original budget was ≤ 80 lines; M7's actual delta was +13 LOC, not the originally-estimated ~6 LOC, because the new `-o <path>` argument needs both `-o<path>` glued + `-o <path>` separated parsing branches AND the usage-string expansion documenting all three dispatch shapes. Final file: 102 lines. The ≤ 110 line budget gives headroom for future per-`-emit=*` glue without re-amendment).

#### P-VEN — vendoring (Story 2)

- **FR-009**: Four audited projects (`cpu16`, `mips32_single_cycle`, `ahb_lite_nsl`, `turboV`) MUST be vendored under `test/audited/<project>/`. Vendoring is **verbatim file copy** of the upstream NSL source files into the directory — not a git submodule, not a `git clone` script, not a `FetchContent_Declare`, not an `ExternalProject_Add`. The Constitution Principle V deterministic-build-environment clause makes any network-dependent vendoring mechanism non-compliant. **Corpus narrowing (2026-05-12 amendment)**: the original spec listed 7 projects; the license audit at M7 implementation T046 surfaced 3 incompatible projects (`rv32x_dev`: GPL-3.0; `mmcspi` + `SDRAM_Controler`: forks with no upstream license + no original-author-grant path). They are dropped from M7's acceptance gate and may be re-added via routine vendoring PRs once their upstream licensing is resolved, per `CONTRIBUTING.md §2.1` + `audited-corpus.contract.md §8`. The four-project corpus preserves the milestone's load-bearing-ness: it covers the full M6 op-coverage surface (combinational + sequential + FSM + sim-only + struct-typed + CPU-control-flow) without the 3 dropped projects, since cpu16 + mips32_single_cycle exercise the CPU-skeleton shape, ahb_lite_nsl exercises the bus-protocol shape, and turboV exercises the full RISC-V-class CPU + Python reference simulator path.
- **FR-010**: Each `test/audited/<project>/PROVENANCE.md` MUST record (at minimum, as machine-parseable `Key: Value` lines): `Upstream-URL`, `Upstream-SHA` (40-character hex), `License` (SPDX identifier or full name), `Vendored-At` (ISO-8601 date), and a `Notes:` block documenting any vendor-time modifications (formatting, license-header additions). If no modifications were made, `Notes: none` is acceptable.
- **FR-011**: Each vendored project's `License` MUST be compatible with Apache-2.0 WITH LLVM-exception (the project's own license). The four projects on the list have been pre-vetted; future additions require an LLVM-exception compatibility audit on the vendoring PR.
- **FR-012**: Updating an audited project to a newer upstream commit MUST be a fresh re-vendoring commit that re-copies files and updates `Upstream-SHA` + `Vendored-At` in `PROVENANCE.md`. Not a submodule bump.
- **FR-013**: A CI lint check MUST assert (a) the seven directories exist, (b) each has a `PROVENANCE.md` with the required `Key:` lines populated, (c) no `.gitmodules` entry references `test/audited/`, (d) no CMake `FetchContent`/`ExternalProject` invocation depends on `test/audited/` paths. Failures block the PR (Constitution Principle IX).

#### P-VCD — golden VCDs (Story 3)

- **FR-014**: Each vendored project MUST carry at least one `test/audited/<project>/golden/<scenario>.vcd` file recording the expected simulation trace for that project's testbench(es). A project with multiple scenarios (e.g. `rv32x_dev/golden/add.vcd`, `rv32x_dev/golden/load.vcd`, …) ships one VCD per scenario.
- **FR-015**: Each `test/audited/<project>/golden/REGEN.md` MUST document: (a) the regeneration command line, (b) the external source (upstream NSL toolchain version, OR a manually-authored testbench reference, OR a formal-framework export), (c) the simulator under which the VCD was recorded (Icarus Verilog version, Verilator version, or the upstream NSL toolchain's bundled simulator), (d) any environment/version dependencies.
- **FR-016**: No golden VCD may be regenerated from `nslc`'s own emitted Verilog. The regeneration command in `REGEN.md` MUST NOT invoke `nslc`. A CI lint check MUST grep for `nslc` invocations inside `test/audited/*/golden/REGEN.md` and fail the PR if any are found.
- **FR-017**: For each of the three CPU projects in the M7 corpus (`cpu16`, `mips32_single_cycle`, `turboV`), the golden VCD's provenance MUST be either (a) a manually-authored reference trace from a known-good instruction stream + cycle-accurate hand-traced expected register/memory updates, or (b) a formal-validation framework export (e.g. riscv-formal trace export — applicable to `turboV` once M8 lands; pre-M8, manually-authored is the only option). **Original cpu-project list narrowed (2026-05-12 amendment per FR-009)**: `rv32x_dev` was dropped from the M7 corpus per the license audit; if it re-enters via a future routine vendoring PR (after upstream relicense), this FR's project list grows accordingly. turboV's golden source is the vendored upstream Python reference simulator under `test/audited/turboV/simulator/ref_sim.py` — that simulator IS the manually-authored reference path per (a).

#### Audited-corpus regression (Story 4)

- **FR-018**: A new CMake target `check-audited` MUST exist that runs the audited-corpus regression for all four projects under both Icarus Verilog and Verilator. The target is also part of the top-level `check` target (so `cmake --build build --target check` covers it).
- **FR-019**: For each of the four projects, the regression MUST: (a) invoke `nslc -emit=verilog` over the project's NSL sources, (b) compile the emitted Verilog + the project's testbench under Icarus Verilog, run the resulting binary, capture VCD; (c) compile the emitted Verilog + the project's testbench under Verilator, run the resulting binary, capture VCD; (d) compare each captured VCD against the project's `golden/*.vcd` per the equivalence criterion (Clarifications Q2).
- **FR-020**: The regression MUST be hosted in lit (`test/audited/lit.cfg.py` extends the top-level `test/lit.cfg.py`) and integrate with the existing `ninja check` workflow. Per Constitution Principle IX, the regression runs in the same CI matrix cell as the rest of the lit suite.
- **FR-021**: The equivalence comparison policy is pinned by Clarifications Q2 → B. Implementation lands a `tools/vcd_diff.py` helper (Python 3.11+ stdlib only; no PyPI deps). The helper accepts two VCD paths and an optional `--signal-map=<path>` argument (consumed when a project ships `golden/SIGNAL_MAP.toml` to alias upstream-vs-`nslc` signal-name mismatches). Behavior: parses both VCDs, ignores `$date`/`$version`/`$timescale`/`$comment` declarations, extracts `$var` signal sets as `(scope-path, name, width)` tuples, applies any signal-map aliases, compares value-change-record sequences on the matched-signal intersection. Exits zero on semantic-equal; non-zero on divergence with a human-readable first-divergence report to stderr (FR-022 hook).
- **FR-022**: A regression failure MUST emit (to stderr and to a per-scenario log file under `build/test/audited/<project>/<scenario>.log`) a diff-formatted explanation of *why* the comparison failed — which signals diverged, at which simulation timestamps. Blind PASS/FAIL output is non-compliant with Constitution Principle V (inspectable pipeline).
- **FR-023**: Wall-clock budget: the full audited-corpus regression target completes in ≤ 15 minutes on a standard CI runner inside the dev container. Per-project parallelism is allowed (lit handles this natively). If a project's regression exceeds 5 minutes individually, the project's `golden/REGEN.md` records the breakdown for future maintainers.

#### Determinism + diagnostics + CI integration (cross-cutting)

- **FR-024**: A FileCheck fixture MUST assert byte-identical output across two consecutive `nslc -emit=verilog` invocations for at least one representative source file under `test/Driver/emit_verilog/determinism/`. The fixture is *positive*: it asserts equality, not just lack of failure (Principle V).
- **FR-025**: The M7 PR MUST NOT use `--no-verify`, `--no-gpg-sign`, or any commit-hook-bypass flag (Constitution Principle IX). All artifacts on the M7 PR are produced by CI from source.
- **FR-026**: The dev-container infrastructure MUST be extended per Clarifications Q3 → A. The existing `Dockerfile.dev` adds Verilator (pinned to a specific upstream tag, e.g. `v5.x`; SHA tracked in the `publish-images` lockfile), `riscv-tests` binaries for the CPU projects (`rv32x_dev` + `turboV`), and any other vendored-project transitive simulation deps surfaced during P-VEN. The image bump lands via the established `PARENT_IMAGE` build-arg pattern (`project_publish_images_buildx_isolation.md`) as either a separate prep PR (preferred) or a self-contained leading commit on the M7 PR. The new image is tagged (e.g. `ghcr.io/koyamanX/nsl-nslc:dev-m7`, or bump the `:dev` rolling tag — exact pin policy decided at `/speckit-plan`). CI's lit cell pins the new tag.

### Key Entities *(include if feature involves data)*

- **`CompileOptions::EmitKind::Verilog`** (existing field): The enum tag for the `-emit=verilog` stage. Declared in `docs/design/nsl_compiler_design.md` §11 line 1317; M7 supplies the dispatch in `Compilation::run` (line 1341) so this tag is dispatched to `runCIRCTPasses → emit`.
- **`Compilation::runCIRCTPasses`** (existing signature; M7 supplies body): `mlir::LogicalResult runCIRCTPasses(mlir::ModuleOp)`. Invokes the stock CIRCT pass pipeline (FSM-lower → seq-lower → SV-prepare). Implementation lives in `lib/Driver/RunCIRCTPasses.cpp` (new file).
- **`Compilation::emit`** (existing signature; M7 supplies body): `mlir::LogicalResult emit(mlir::ModuleOp)`. Invokes `circt::exportVerilog` / `circt::exportSplitVerilog` based on `-o` argument shape. Implementation lives in `lib/Driver/EmitVerilog.cpp` (new file).
- **`include/nsl/Driver/EmitVerilog.h`**: New public header, one symbol (`nsl::driver::emitVerilog`), mirroring `EmitHW.h`'s shape.
- **`test/audited/<project>/`** (seven directories): Vendored audited NSL projects. Each contains `*.nsl` files (verbatim from upstream), `PROVENANCE.md`, and a `golden/` subdirectory.
- **`PROVENANCE.md`** (seven files): Per-project provenance manifest. Machine-parseable `Key: Value` shape (fields enumerated in FR-010).
- **`golden/<scenario>.vcd`** (one or more per project): External-source-derived golden VCDs. Source rule per FR-016.
- **`golden/REGEN.md`** (one per project): Regeneration recipe. Fields enumerated in FR-015.
- **`tools/vcd_diff.py`**: New Python helper for semantic-equal VCD comparison (FR-021). Stdlib-only (Python 3.11+), no PyPI deps. Becomes the project's canonical VCD-equivalence definition reusable post-M7 for ad-hoc debugging.
- **`test/audited/<project>/golden/SIGNAL_MAP.toml`** (optional, per-project): Signal-name aliasing table for projects where upstream-NSL-toolchain and `nslc`-emitted Verilog name a signal differently (e.g. flatten/preserve of struct-field naming). Empty or absent ⇒ verbatim signal-name match expected (the default).
- **`check-audited` CMake target**: New top-level target running the audited-corpus regression (FR-018).

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: All four vendored audited projects (`cpu16`, `mips32_single_cycle`, `ahb_lite_nsl`, `turboV`) build via `nslc -emit=verilog` and pass the semantic-equal VCD-equivalence comparison against their golden VCDs under **both** Icarus Verilog and Verilator simulators. 8 simulator/project cells, 8 PASS. **Cell count narrowed from 14 → 8 per FR-009 amendment (2026-05-12)**: the original 7-project × 2-simulator matrix narrows to 4-project × 2-simulator after the license audit. The load-bearing-ness criterion (SC-007) is preserved: a deliberate breakage of any M-track library still causes at least one of the 8 cells to fail, since the 4-project corpus covers the full op-coverage surface (per FR-009 narrative).
- **SC-002**: Two consecutive invocations of `nslc -emit=verilog <input> -o <output>` with identical inputs, flags, environment, and toolchain version produce byte-identical `<output>` files for every source in the M3/M5/M6/M7 test corpus.
- **SC-003**: Every vendored audited project has a complete `PROVENANCE.md` whose required fields (URL, SHA, License, Vendored-At) can be parsed by a future maintainer in under 30 seconds per project to reconstruct the vendoring snapshot.
- **SC-004**: Every golden VCD has a `REGEN.md` peer whose regeneration command can be executed by a future maintainer to reproduce the golden from its external source (no information loss between original capture and committed recipe).
- **SC-005**: A new contributor can run the full audited-corpus regression locally inside the dev container in under 15 minutes wall-clock on a standard development workstation (Linux x86_64, 8 cores, 16 GB RAM).
- **SC-006**: Adding a new audited project to the corpus post-M7 is a routine single-PR change: vendor sources + `PROVENANCE.md` + `golden/*.vcd` + `golden/REGEN.md`, with no edits to `lib/Driver/`, no edits to `tools/nslc/`, and no edits to `test/lit.cfg.py`. The infrastructure carries the new project automatically once it lands under `test/audited/<project>/`.
- **SC-007**: A deliberate breakage of any M-track library (e.g. revert one M6 conversion pattern) causes at least one of the 8 simulator/project regression cells to fail — the regression is *load-bearing*, not trivially-pass. (Verified once by a one-shot reverse-test during M7 implementation, then trusted thereafter.)
- **SC-008**: Diagnostics from anywhere in the `nslc -emit=verilog` pipeline (preprocessor, parser, Sema, nsl-lower, CIRCT passes, ExportVerilog) appear in exactly one stream (stderr or JSON-on-stdout, per existing `--diagnostic-format` flag), with no leaked MLIR/CIRCT internal diagnostics escaping to a second channel.

## Assumptions

- The seven audited NSL projects in the corpus are stable in their feature surface as of the M7 vendoring snapshot — the M3 Sema layer, M5 structural passes, and M6 conversion patterns are sufficient to lower them to CIRCT IR without invoking any NSL feature scheduled for a post-M7 milestone. (This is the working assumption of `docs/CLAUDE.md` §1's feature roll-up; if a vendored project uses a yet-unsupported NSL feature, the resolution is to amend the M-track roadmap or drop the project, not to land partial M7.)
- The vendored CIRCT version in `ghcr.io/koyamanX/nsl-nslc:dev` ships `circt::exportVerilog`, `circt::exportSplitVerilog`, `circt::fsm::createConvertFSMToSVPass`, `circt::seq::createLowerSeqToSVPass`, and `circt::sv::createPrepareForEmissionPass` as C++ API entries — these are all CIRCT-main symbols as of the 2026-Q2 LLVM/CIRCT vendoring (the M6 spec's research.md confirmed these dialects + ExportVerilog are present).
- The dev container's Icarus Verilog is recent enough to accept SystemVerilog-2012 (`-g2012`) syntax for the CIRCT-emitted output (per FR-019). The audited-corpus testbenches are themselves SystemVerilog-2012-compatible (the upstream NSL toolchain emits SystemVerilog-2012).
- Verilator's `--lint-only` mode is acceptable as a syntactic-acceptance check independent of running the binary, but the regression invokes Verilator-as-simulator (build the C++ harness, run, capture VCD) rather than `--lint-only` for the equivalence comparison.
- The reverse-test for SC-007 (deliberate breakage) is a one-time validation during M7 implementation, not a permanent CI cell. Maintaining a "must-break-on-revert" cell forever is impractical; the one-time check at M7-landing suffices to establish that the regression is load-bearing.
- The Clarifications Q1/Q2/Q3 answers (file-organization, equivalence criterion, container surface) are resolved in the 2026-05-11 session above and pinned into FR-003, FR-021, FR-026 respectively; `/speckit-plan` builds against those answers.
