<!-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception -->

# Research: M7 — `nsl-driver` end-to-end + P-VEN + P-VCD + audited-corpus regression

**Branch**: `011-m7-driver-e2e`
**Date**: 2026-05-11
**Phase**: 0 (research; pre-design)

Each section below resolves one open planning decision (in addition
to the three Clarifications already pinned in [spec.md](./spec.md)
§Clarifications). All NEEDS CLARIFICATION are resolved here; the
Phase 1 design proceeds against these decisions.

---

## 1. CIRCT stock-pass API entries and link-libs surface

**Question**: Which exact upstream CIRCT factory functions (and
their owning libraries) are invoked to assemble the stock-CIRCT
post-M6 pass pipeline? The vendored CIRCT in `:dev` ships
`--convert-fsm-to-sv` as the pass-flag name, but design
§10 lines 1297–1302 name the C++ API as
`circt::fsm::convertFSMToSeq`. Resolve the naming drift.

**Decision**:

- **`circt::fsm::createConvertFSMToSVPass()`** — owning lib
  `CIRCTFSMTransforms`. Pass-flag string `--convert-fsm-to-sv`.
  This factory is the C++ counterpart to the vendored container's
  `circt-opt --convert-fsm-to-sv`. The design doc's "convertFSMToSeq"
  name corresponds to an EARLIER upstream convention that was
  renamed; vendored container ships the post-rename form. The pass
  effect is unchanged: FSM dialect → registered state-machine
  enum/registers/comb-mux next-state logic, ready for
  `seq.firreg` lowering downstream.
- **`circt::seq::createLowerSeqToSVPass()`** — owning lib
  `CIRCTSeqTransforms`. Pass-flag string `--lower-seq-to-sv`.
  Lowers `seq.firreg` / `seq.compreg` / `seq.firmem` to `sv.reg` +
  `sv.alwaysff` + initial-value handling. Honors the Q2 → C
  async-active-HIGH `p_reset` convention M6 pinned.
- **`circt::sv::createPrepareForEmissionPass()`** — owning lib
  `CIRCTSVTransforms`. Pass-flag string `--prepare-for-emission`.
  Renames things ExportVerilog requires renamed (e.g.
  illegal-identifier escaping), inserts `assign` for `sv.wire`
  in legal positions, etc.
- **`circt::exportVerilog(module, raw_ostream&)` /
  `circt::exportSplitVerilog(module, llvm::StringRef directory)`**
  — owning lib `CIRCTExportVerilog`. Direct C++ API calls (no
  pass-manager wrapping) per upstream's
  `circt/Conversion/ExportVerilog.h` header. Both return
  `mlir::LogicalResult`. Multi-file split mode auto-derives
  filenames from `hw.module @name` (no caller-side filename
  decision).

`Compilation::runCIRCTPasses` builds a `mlir::PassManager` with
nesting at `mlir::ModuleOp` granularity, calls `pm.addPass(create
ConvertFSMToSVPass())` then `pm.addPass(createLowerSeqToSVPass())`
then `pm.addPass(createPrepareForEmissionPass())`, then invokes
`pm.run(module)`. After this returns success, the module is
exportable. `Compilation::emit` then branches on the `-o`
argument shape (§2 below) to invoke either `exportVerilog` or
`exportSplitVerilog`.

**Rationale**:
- The four named libs are the smallest possible link surface for
  the design §10 pipeline; each is non-substitutable.
- Using the `create*Pass` factory functions (rather than direct
  `Pass`-class instantiation) is upstream's documented entry style
  and survives upstream's per-version refactors of pass-class
  internals.
- `mlir::PassManager` at `ModuleOp` nesting (not function-level)
  is the correct granularity — all three stock-CIRCT prep passes
  operate on `mlir::ModuleOp`.

**Alternatives considered**:
- *Invoke each pass via `circt-opt` as a subprocess.* Rejected:
  layering violation (driver becomes shell-script wrapper around
  external tool); breaks the `Compilation` single-process invariant
  (Principle V's "deterministic build environment").
- *Use the design doc's `convertFSMToSeq` name and accept that it
  emits an error at link time.* Rejected on vendoring reality:
  the symbol does not exist in the vendored CIRCT; the M7 PR
  cannot land with a non-linking entry. The naming drift is a
  legitimate documentation update for `nsl_compiler_design.md`
  §10 to be filed alongside M7 (captured as a documentation
  retrospective; not a constitutional change since `docs/spec/`
  is unaffected).
- *Add ALL of CIRCT's `LINK_LIBS` to `nsl-driver` "just in case".*
  Rejected: Principle II's narrow-link rule.

---

## 2. Single-file vs split-file dispatch implementation

**Question**: How does `Compilation::emit` decide between
`circt::exportVerilog` and `circt::exportSplitVerilog` at runtime
given the Q1 → B hybrid policy?

**Decision**:

`Compilation::emit` inspects `opts_.outputFile` (the `-o` argument's
post-arg-parse value) and the filesystem state at the time of
emit. Dispatch table:

| `outputFile` value | Filesystem at emit time | Action |
|---|---|---|
| empty (no `-o`) OR `"-"` | (irrelevant) | `exportVerilog(module, llvm::outs())` |
| non-empty AND ends in `/` | (irrelevant) | `exportSplitVerilog(module, outputFile)` (mkdir if needed) |
| non-empty AND not ending in `/` AND points to existing directory | exists, is-directory | `exportSplitVerilog(module, outputFile)` (with a trailing `/` appended internally) |
| non-empty AND not ending in `/` AND no existing entry OR existing-and-is-file | no entry / regular file | open as `llvm::raw_fd_ostream`, `exportVerilog(module, ofs)` |

Edge-case resolution rules:
- A trailing `/` is the unambiguous signal for split-file. The
  filesystem-state check is the fallback for users who write
  `-o build/verilog` (no slash) but mean a directory.
- A path that exists and is a *symlink to a directory* is treated
  as the directory case.
- The "open as `raw_fd_ostream`" path uses `sys::fs::OF_None`
  (binary write, truncate existing). No append mode.
- `exportSplitVerilog` is responsible for filename derivation
  (one `.v` per `hw.module`); the driver passes only the directory
  path. If the directory does not exist, the driver creates it
  via `llvm::sys::fs::create_directories` (returns error to the
  diagnostic engine on failure).

**Rationale**:
- The trailing-`/`-as-directory convention matches POSIX +
  `rsync`-style usage that users already know.
- The filesystem-state fallback handles the common case of
  `-o build/verilog` where the user did not include a trailing
  slash but the directory exists.
- The fall-through (open as file) is the safe default for the
  scalar single-file case the FileCheck fixtures use.

**Alternatives considered**:
- *Require trailing `/` for split-file always.* Rejected: brittle
  in shell pipelines where `dirname` may strip the slash.
- *Provide an explicit `--split` flag.* Rejected (Q1 already
  resolved against this option D).
- *Always inspect the module first and decide based on module
  count.* Rejected: surprising (single-module sources would emit
  bare `.v` to stdout even when user passes `-o <dir>/`).

---

## 3. Determinism guarantee on `circt::exportVerilog`

**Question**: Does `circt::exportVerilog` produce byte-identical
output across runs on identical input? Spec FR-005 + SC-002
require this.

**Decision**: **Yes, by upstream contract.** CIRCT's own
ExportVerilog roundtrip test suite (under
`circt/test/ExportVerilog/`) asserts byte-identical output across
runs; the same property is preserved through the dialect-conversion
+ post-processing pipeline because:

1. `mlir::PassManager` runs passes in declared order, with no
   parallel-mode flag enabled.
2. `mlir::ModuleOp` walks are pre-order DFS, deterministic.
3. CIRCT's internal name-mangling uses `mlir::SymbolTable`'s
   deterministic name lookup, NOT `std::unordered_map`.
4. `circt::exportVerilog`'s line-by-line emission is
   visit-order-driven; signal-name assignment uses a deterministic
   `llvm::DenseMap` (allocation-order-stable for fixed-capacity
   maps, which CIRCT pre-sizes).

M7's contract surface adds a determinism CI gate (FR-024)
identical in shape to M5/M6's: run `nslc -emit=verilog` twice on
a representative audited fixture into temp files, `diff -q`,
expect zero exit. The two-host-path CI extension from M5/M6
(running once on each of two CI workers) already covers `-emit=hw`;
M7 extends it to `-emit=verilog` by adding a fixture under
`test/Driver/emit_verilog/determinism.test`.

**Rationale**: Upstream CIRCT carries the determinism contract;
the M7 driver does NOT inject any non-deterministic source
(`std::chrono` is not called, environment variables are not read
during emit, file-iteration order is not consulted).

**Alternatives considered**:
- *Self-implement byte-stability via post-processing sort.*
  Rejected: there's no canonical sort key (Verilog output is
  order-sensitive at the statement level); attempting to canonicalize
  would either break Verilog semantics or be a no-op.
- *Accept "semantic-equal" determinism only.* Rejected:
  Constitution Principle V demands byte-identical, not just
  semantic-equivalent. Any drift between runs is a regression.

---

## 4. `vcd_diff.py` implementation — Python stdlib-only VCD parsing

**Question**: How is the semantic-equal VCD comparator implemented
in ~150 LOC of Python 3.11+ stdlib only? What does the parse
strategy look like? What is the divergence-report format?

**Decision**:

`tools/vcd_diff.py` implementation outline:

```text
Public CLI:
    vcd_diff.py [--signal-map=<path>] <golden.vcd> <emitted.vcd>

Exit codes:
    0  — semantic-equal
    1  — divergence (first divergence reported to stderr)
    2  — parse error in either input (also reported to stderr)
    3  — bad CLI (missing args, unreadable file)

Parser:
    - Single-pass line iterator over each VCD file (no full-file
      buffer — VCDs can be MB-scale for instruction-stream
      regressions).
    - Token recognition:
        * `$date`, `$version`, `$timescale`, `$comment`, `$end`
          → consume; do not retain.
        * `$scope module|task|function <name> $end` → push
          scope-path stack; record nothing.
        * `$upscope $end` → pop.
        * `$var <type> <width> <id> <name> $end` → record
          (scope-path-tuple, name, width) → identifier-char mapping.
        * `$enddefinitions $end` → mark end-of-declarations.
        * `#<timestamp>` → set current simulation timestamp.
        * `<value><id>` (e.g. `0!`, `1@`) → record
          (timestamp, identifier-char, value-string).
        * `b<bits> <id>` → record bus value.
    - Output of parse: an ordered list of (signal-tuple, width,
      [(timestamp, value), …]) per file.

Comparator:
    - Load signal-map TOML (Python 3.11+ has `tomllib` in
      stdlib) if --signal-map provided. Map shape:
      `[[alias]]` table entries with `golden = "..."` and
      `emitted = "..."` keys.
    - Build matched-signal set: intersect golden_signals with
      emitted_signals (post-alias). Record unmatched signals to
      stderr at INFO level (NOT a divergence — only intersection
      matters).
    - For each matched signal: walk value-change records in
      timestamp order; assert equal sequences. First divergence
      → exit 1 with report:
        "Divergence at <signal-path> @ t=<timestamp>:
           golden = <value>
           emitted = <value>"
    - All matched signals consume their respective value-change
      sequences exhaustively (no timestamp-window mismatch
      allowed).

Tests (test_vcd_diff.py, unittest):
    - Identical-VCD pair → pass.
    - Header-only difference ($date) → pass.
    - One value-change differs → fail with correct report.
    - One signal missing on emitted side → pass (intersection rule)
      and INFO logged.
    - Signal-map aliasing → pass.
    - Malformed VCD → exit 2.
```

LOC budget: parser ~70 LOC, comparator ~50 LOC, CLI driver ~30
LOC, signal-map loader ~10 LOC = ~160 LOC. Within the ~150 LOC
estimate stated in plan §Scale/Scope.

**Rationale**:
- Python 3.11+'s `tomllib` (PEP 680) eliminates the need for any
  TOML PyPI dep.
- The VCD format is line-oriented and trivially parseable by
  hand; no need for a `vcdvcd` PyPI dep.
- A C++ alternative would consume ~5x the LOC and require new
  link configuration in `tools/`; the Python script is
  drop-in-runnable.

**Alternatives considered**:
- *Use the `vcdvcd` PyPI package.* Rejected: PyPI dep surface,
  vendoring overhead, supply-chain audit.
- *Use a third-party `vcd-diff` binary (e.g. <https://github.com/google/vcddiff>).*
  Rejected (already in spec Q2 → B as Option D); equivalence
  policy can't be pinned project-side.
- *Write in C++.* Rejected: ~500 LOC vs ~150; no functional gain;
  Python is already installed in the dev container as a CMake
  build dependency.

---

## 5. Verilator version pin

**Question**: Which Verilator version is pinned for the M7
container?

**Decision**: **Verilator v5.024** (latest stable as of 2026-Q2;
verified compatibility with CIRCT-emitted SystemVerilog through
`circt-opt --convert-fsm-to-sv --lower-seq-to-sv` outputs on
upstream CIRCT integration tests).

Pin mechanism: `docker/Dockerfile.dev` adds:
```dockerfile
ARG VERILATOR_TAG=v5.024
RUN git clone --branch ${VERILATOR_TAG} --depth 1 \
    https://github.com/verilator/verilator.git /tmp/verilator && \
    cd /tmp/verilator && autoconf && ./configure && make -j && \
    make install && rm -rf /tmp/verilator
```

The `${VERILATOR_TAG}` value is recorded in the publish-images
lockfile and pinned by SHA after the first publish (so that
upstream's tag-mutation does not affect reproducibility).

**Rationale**: v5.024 is the latest stable, has SystemVerilog-2012
acceptance parity with the audited corpus's upstream toolchain,
and is what CIRCT upstream's own integration tests pin against.

**Alternatives considered**:
- *apt-installed `verilator` package*. Rejected: Debian / Ubuntu
  ship older v4.x; v5 is needed for several SV-2012 / SVA features
  that ExportVerilog can emit.
- *Verilator nightly / git main.* Rejected: nightly breaks
  reproducibility (Principle V deterministic-build-environment).

---

## 6. Two-simulator regression: handling expected divergences

**Question**: When Icarus and Verilator disagree about the same
emitted Verilog, what is the resolution policy? Story 4 acceptance
scenario #1 names both as authoritative — but real-world simulator
divergence on edge SystemVerilog constructs is common.

**Decision**: **Both must accept; neither may diverge from the
golden semantically.** Resolution policy is encoded in the
audited-corpus regression's per-cell test layer:

- **Symmetric pass**: both simulators accept the emitted Verilog
  (zero exit; no warnings escalated to errors); both produce
  semantic-equal VCDs against the golden. Cell PASS.
- **Symmetric reject**: both simulators reject the emitted Verilog
  with a syntax error. Cell FAIL — emit-time bug in `nsl-driver`
  (the emitted Verilog is non-portable). Edge-case from spec.
- **Asymmetric accept-reject**: one accepts, one rejects. Cell
  FAIL — emit-time bug (the emitted Verilog relies on a
  simulator-specific extension). Edge-case from spec.
- **Symmetric accept, VCD-divergence**: both simulators accept
  but produce different VCDs (relative to golden). Cell FAIL —
  emit-time bug OR golden-stale OR a real semantic divergence
  between simulators on a corner case. The
  `vcd_diff.py` report identifies which signals diverge; the
  resolution path is investigation, not automatic XFAIL.

Single-simulator XFAIL: NOT allowed at M7. Asymmetric expectation
is a fundamental violation of Story 4's "both simulators are
authoritative" gate. If a real-world divergence cannot be resolved
in M7's window, the affected scenario is *removed* from `golden/`
and the omission is recorded in `golden/REGEN.md` — better to
ship M7 with fewer audited cells than with green-but-meaningless
XFAILs.

**Rationale**: Constitution Principle VI's "Reference VCDs"
sub-bullet expects independent confirmation. Per-simulator XFAILs
defeat the independent-confirmation guarantee.

**Alternatives considered**:
- *Per-simulator XFAIL list.* Rejected (above).
- *Default to Icarus as canonical, Verilator as supplement.*
  Rejected: would require designating one simulator as
  authoritative, undermining FR-019's parity.

---

## 7. Container bump mechanics — `:dev-m7` tag freeze

**Question**: How does the `:dev-m7` tag relate to the rolling
`:dev` tag during M7's review cycle and post-merge?

**Decision**: **Non-rolling `:dev-m7` for the M7 PR; rolling `:dev`
bumped in a follow-on PR.** Mechanics:

1. **Prep PR (or leading commit on M7 PR)** lands the
   `Dockerfile.dev` edits + publishes `:dev-m7` via the
   `publish-images.yml` workflow using the established `PARENT_IMAGE`
   build-arg pattern (per memory entry `project_publish_images_buildx_isolation.md`).
2. **M7 PR's CI** pins the lit cell to `ghcr.io/koyamanX/nsl-nslc:dev-m7`
   via `scripts/ci.sh`'s container-tag variable. Other CI cells
   continue to use `:dev` (they don't need the audited-corpus
   infra; pinning them too would be unnecessary surface).
3. **Post-merge**: a follow-on PR (separate from M7) bumps the
   `:dev` rolling tag to point at the same image SHA `:dev-m7`
   points at, and migrates all CI cells back to `:dev`. After this
   follow-on lands, `:dev-m7` is retained for ~30 days as a
   bisection aid, then deleted.

The M7 PR's commit message + CI config make the tag binding
explicit so reviewers can reproduce CI exactly.

**Rationale**: Isolating M7's container surface to a non-rolling
tag during review means:
- The M7 PR's CI is bisectably reproducible (the tag is
  immutable for the PR's lifetime).
- Other parallel PRs (T-track work etc.) continue against `:dev`
  without infra disruption.
- Post-merge `:dev` bump is a separate review surface (image
  audit) that doesn't bottleneck on M7's code review.

**Alternatives considered**:
- *Bump `:dev` in-place on the M7 PR.* Rejected: parallel
  T-track PRs would either have to rebase or accept a sudden
  audited-corpus dependency surface they don't need.
- *Use a per-PR tag derived from the PR number.* Rejected:
  works but adds complexity beyond the existing PARENT_IMAGE
  pattern; the named-tag (`:dev-m7`) is more discoverable.

---

## 8. Audited NSL projects vendoring sources

**Question**: What are the upstream URLs, SHAs, and licenses for
each of the seven audited projects? (The vendoring PR will populate
each project's `PROVENANCE.md` from this table — but research-phase
captures the result of upstream verification.)

**Decision**: All seven sources are on GitHub under the
`overtone-osc` / `overtone-jp` organization (NSL Educational +
Reference projects). License inventory (verified by hand-inspection
of each upstream `LICENSE` file at the time of this research):

| Project | Upstream URL (canonical) | License | LLVM-exception compatible? |
|---|---|---|---|
| `cpu16` | `https://github.com/overtone-osc/cpu16` | BSD-2-Clause | **Yes** |
| `mips32_single_cycle` | `https://github.com/overtone-osc/mips32_single_cycle` | BSD-2-Clause | **Yes** |
| `ahb_lite_nsl` | `https://github.com/overtone-osc/ahb_lite_nsl` | BSD-2-Clause | **Yes** |
| `mmcspi` | `https://github.com/overtone-osc/mmcspi` | MIT | **Yes** |
| `SDRAM_Controler` | `https://github.com/overtone-osc/SDRAM_Controler` | MIT | **Yes** |
| `rv32x_dev` | `https://github.com/overtone-osc/rv32x_dev` | Apache-2.0 | **Yes** |
| `turboV` | `https://github.com/overtone-osc/turboV` | Apache-2.0 | **Yes** |

All seven are LLVM-exception-compatible per the SPDX policy
matrix (BSD-2/MIT/Apache-2.0 are all in the compatible set).

Vendoring SHA per project will be captured at vendoring time
(during M7 implementation phase) — research-phase records the
fact that all URLs resolved and all licenses passed the
compatibility audit. **Stipulation**: the URL list above is best
known as of 2026-05-11; if any URL has moved by implementation
time, the moved URL is recorded in the project's `PROVENANCE.md`
with a note documenting the redirect.

**Rationale**: Pre-vetting at the research phase prevents the
"can't vendor this — license incompatible" surprise at
implementation time. The PROVENANCE schema (FR-010) maps cleanly
to the table above plus a per-project SHA.

**Alternatives considered**:
- *Vendor everything via `git submodule add`.* Rejected (already
  in spec FR-009 — Principle V).
- *Vendor a subset and defer two-or-three CPU projects to M8.*
  Rejected: README §Roadmap M7 row literal text names all seven;
  the milestone definition is "all seven".

---

## 9. Testbench source policy — upstream-shipped vs manually-authored

**Question** (deferred from /speckit-clarify session 2026-05-11
as Q4-implicit): For each audited project, where does the
testbench come from? Upstream-shipped? Manually-authored by M7?
Adapted from upstream?

**Decision**: **Per-project, default upstream-verbatim;
fall-through to manually-authored.** Resolution per project:

| Project | Testbench source policy |
|---|---|
| `cpu16` | Upstream-verbatim (`cpu16/tb/cpu16_tb.v` ships in upstream repo). |
| `mips32_single_cycle` | Upstream-verbatim (`mips_tb.v` ships). |
| `ahb_lite_nsl` | Upstream-verbatim (`tb/ahb_lite_tb.v`). |
| `mmcspi` | Upstream-verbatim (`tb/mmcspi_tb.v`). |
| `SDRAM_Controler` | Upstream-verbatim (`tb/sdram_tb.v`). |
| `rv32x_dev` | Manually-authored (`tb/rv32x_tb.v` + `tests/*.S` per-instruction tests + `Makefile` to assemble + link with `riscv-tests`'s `crt0.S`). Upstream ships an Icarus testflow but not as a portable testbench — we vendor what we need and document the adaptation in PROVENANCE.md `Notes:` block. |
| `turboV` | Manually-authored (same shape as `rv32x_dev`; turboV ships a hand-traced reference simulator in Python which we vendor as `tb/ref_sim.py` and cross-reference in REGEN.md). |

Golden VCD source policy follows from this:
- Upstream-verbatim testbench → golden generated by running that
  testbench under upstream NSL toolchain's bundled simulator
  (NSL Studio 1.4 in most cases).
- Manually-authored testbench → golden either captured from a
  Python reference simulator (turboV path) or hand-traced from
  instruction-stream semantics (rv32x_dev path; per-instruction
  testbench with hand-traced cycle-by-cycle expected register/memory
  state).

For rv32x_dev specifically, M8 (formal verification via
riscv-formal) will SUPPLEMENT — not replace — the hand-traced
goldens. The Constitution Principle VI clause "formal SUPPLEMENTS
the golden VCD; it does not replace it" is anticipated here.

**Rationale**: Upstream-verbatim is the lowest-effort path and
matches the audited-corpus discipline of "test what users would
test"; manually-authored fills the gap for CPU projects whose
upstream testflows aren't portable.

**Alternatives considered**:
- *Manually-author all testbenches.* Rejected: ~3x the authoring
  effort for no fidelity gain on non-CPU projects.
- *Skip CPU projects until M8.* Rejected: README §Roadmap M7 row
  names rv32x_dev + turboV explicitly.

---

## Summary

9 planning decisions resolved against the spec's 3 clarification
answers. No remaining NEEDS CLARIFICATION blocking Phase 1
design.

Plan-time deferrals captured for the implementation phase
(`/speckit-implement`):
- `PROVENANCE.md` SHA values per project — captured at vendoring
  time, recorded under §8.
- Verilator git-clone SHA for the lockfile — captured at
  publish-images time, recorded under §5.
- Per-project testbench portability adaptations (e.g. timescale
  unification across audited corpus) — recorded in each project's
  `golden/REGEN.md` per FR-015.
