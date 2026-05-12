<!-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception -->

# Data Model: M7 — `nsl-driver` end-to-end + P-VEN + P-VCD + audited-corpus regression

**Branch**: `011-m7-driver-e2e`
**Date**: 2026-05-11
**Phase**: 1 (design)

Entities described here are the on-disk artefacts, in-memory
types, and value-flow contracts that M7 introduces or extends.
The data model is read alongside [contracts/](./contracts/) — this
file lists *what exists*; the contracts pin *how it behaves*.

---

## 1. `CompileOptions::EmitKind::Verilog` (in-memory enum tag)

| Property | Value |
|---|---|
| Owner | `lib/Driver/Compilation.h` (existing field) |
| Type | `enum class EmitKind { Tokens, AST, NSLMLIR, CIRCT, HW, Verilog }` |
| Declared in | `docs/design/nsl_compiler_design.md` §11 line 1317 |
| Default | `Verilog` (per design §11 line 1318) |
| State transitions | N/A (parse-time only — set by `tools/nslc/main.cpp` after `-emit=verilog` argument detection) |
| Validation | Mutually exclusive with other `EmitKind` values (single `-emit=` per invocation) |

**Status change at M7**: existing enum tag is for the first time
dispatched in `Compilation::run`'s switch. M6 left the `Verilog`
case as a fall-through to the stub at `tools/nslc/main.cpp:84`;
M7 wires it through to `runCIRCTPasses → emit`.

---

## 2. `Compilation::runCIRCTPasses` member function

| Property | Value |
|---|---|
| Owner | `lib/Driver/RunCIRCTPasses.cpp` (NEW M7) |
| Signature | `mlir::LogicalResult runCIRCTPasses(mlir::ModuleOp)` |
| Declared in | `docs/design/nsl_compiler_design.md` §11 line 1352 (existing) + `lib/Driver/Compilation.h` (existing — body lands at M7) |
| Inputs | `mlir::ModuleOp` populated entirely by `hw`/`comb`/`seq`/`fsm`/`sv` ops (M6's exit shape) |
| Outputs (success) | Same `mlir::ModuleOp`, now populated entirely by `hw`/`comb`/`sv` ops (no `nsl`/`fsm`/`seq` residue — `fsm` lowered by `convertFSMToSV`; `seq` lowered by `lowerSeqToSV`) |
| Outputs (failure) | `mlir::failure()` plus a diagnostic routed through `basic::DiagnosticEngine` |
| State transitions | Stateless (passes are pure functions over the IR) |
| Validation | Verifier-clean post-condition: the module passes `mlir::verify(module)` after each pass returns success. |

**Internal structure**: builds a `mlir::PassManager` configured
for `ModuleOp` nesting, adds the three passes in order
(`createConvertFSMToSVPass` → `createLowerSeqToSVPass` →
`createPrepareForEmissionPass`), runs the pipeline, returns the
result. The `DiagnosticBridge` RAII handler is scoped to cover
the pass run.

---

## 3. `Compilation::emit` member function

| Property | Value |
|---|---|
| Owner | `lib/Driver/EmitVerilog.cpp` (NEW M7 — note: the public driver-glue function `nsl::driver::emitVerilog` lives here; the `Compilation::emit` member function lives alongside it as a private helper invoked by `emitVerilog`) |
| Signature | `mlir::LogicalResult emit(mlir::ModuleOp)` |
| Declared in | `docs/design/nsl_compiler_design.md` §11 line 1353 (existing) |
| Inputs | `mlir::ModuleOp` post-`runCIRCTPasses` (hw/comb/sv only) |
| Outputs (success) | Verilog bytes written to either a single file, multiple files (split mode), or stdout per the dispatch table (research.md §2) |
| Outputs (failure) | `mlir::failure()` plus a diagnostic |
| Dispatch table | See research.md §2 (driven by `opts_.outputFile` shape + filesystem state) |
| Validation | If split-mode, the target directory either pre-exists or is created via `llvm::sys::fs::create_directories`; failure to create routes through the diagnostic engine |

---

## 4. `nsl::driver::emitVerilog` public entry

| Property | Value |
|---|---|
| Owner | `lib/Driver/EmitVerilog.cpp` (NEW M7) |
| Declared in | `include/nsl/Driver/EmitVerilog.h` (NEW M7) |
| Signature | `int emitVerilog(llvm::StringRef input_path, const EmitTokensOptions &opts, llvm::raw_ostream &os, llvm::raw_ostream &err)` |
| Mirrors | `nsl::driver::emitHW(...)` (M6) signature exactly |
| Exit codes | 0=success, 1=at-least-one-error-severity-diagnostic, 3=input-file-could-not-be-opened (matches `emitHW.contract.md` §3) |
| Buffering rule | "No partial output on error" — same as `emitHW`: bytes are buffered until completion, then written atomically to `os` |
| Behavior | Runs the full pipeline (preprocess → lex → parse → sema → lowerToNSL → runNSLPasses → lowerToCIRCT → runCIRCTPasses → emit); calls into `Compilation` member functions for the stages it doesn't own directly |

---

## 5. `test/audited/<project>/` directory entity (P-VEN)

| Property | Value |
|---|---|
| Cardinality | Exactly 7 directories at M7 acceptance: `cpu16`, `mips32_single_cycle`, `ahb_lite_nsl`, `mmcspi`, `SDRAM_Controler`, `rv32x_dev`, `turboV` |
| Required files | `PROVENANCE.md`, one or more `.nsl` source files, `golden/` subdirectory |
| Optional files | `tb/` (testbench) subdirectory, `tests/` (test inputs) subdirectory, `Makefile`, `README.md` |
| Lifecycle | Created by vendoring PR; updated by re-vendoring PR (fresh file copy, not diff) |
| Validation | Configure-time lint via `cmake/AuditedCorpusLint.cmake` per FR-013 |

State transitions: created → maintained → (re-vendored). No
deletion path expected for the seven canonical projects;
future additions go through their own vendoring PR.

---

## 6. `PROVENANCE.md` entity

| Property | Value |
|---|---|
| Owner | `test/audited/<project>/PROVENANCE.md` (one per project) |
| Format | Markdown with machine-parseable `Key: Value` lines in the header |
| Required keys | `Upstream-URL`, `Upstream-SHA` (40-character hex), `License` (SPDX or full name), `Vendored-At` (ISO-8601 date) |
| Optional keys | `Upstream-Tag`, `Vendored-By` (committer), `Notes:` (multi-line block) |
| Schema validation | `cmake/AuditedCorpusLint.cmake` greps for each required `Key:` line; non-empty value required |
| State transitions | Initial draft at vendoring time → frozen for life of vendoring snapshot → fresh version on re-vendoring |

**Example shape**:
```markdown
# PROVENANCE — cpu16

Upstream-URL: https://github.com/overtone-osc/cpu16
Upstream-SHA: 4f3a91b7e6c2d8a05f1b9e7c4d6a8b3e2f1c5d4a
Upstream-Tag: v1.2.0
License: BSD-2-Clause
License-File: ./LICENSE
Vendored-At: 2026-05-12
Vendored-By: ckoyama

## Notes

- License header inserted as comment in each `.nsl` file per the
  audited-NSL-license-passthrough rule.
- Renamed `top.nsl` → `cpu16_top.nsl` to disambiguate from other
  audited projects' top-level files (cross-project name-collision
  avoidance during cross-project lit-test discovery).
```

---

## 7. `test/audited/<project>/golden/` directory entity (P-VCD)

| Property | Value |
|---|---|
| Owner | `test/audited/<project>/golden/` (one per project) |
| Required files | One or more `<scenario>.vcd`, exactly one `REGEN.md`, optionally one `SIGNAL_MAP.toml` |
| Per-project cardinality | At least 1 `.vcd`. CPU projects (rv32x_dev, turboV) carry one per instruction-family test (~10–15 total per project); non-CPU projects carry 1–3 scenarios |
| Lifecycle | Created during P-VCD authoring → regenerated when `REGEN.md` is invoked (manual operation) |
| Validation | (a) every `.vcd` file has a corresponding `REGEN.md` entry; (b) no `REGEN.md` mentions `nslc` (FR-016); enforced by `cmake/AuditedCorpusLint.cmake` |

---

## 8. `REGEN.md` entity

| Property | Value |
|---|---|
| Owner | `test/audited/<project>/golden/REGEN.md` (one per project) |
| Format | Markdown with required sections |
| Required sections | `## Regeneration command`, `## External source`, `## Simulator + version`, `## Environment / dependencies` |
| Optional sections | `## Notes`, `## Per-scenario` (when multiple scenarios) |
| Constraint | MUST NOT invoke `nslc` (CI lint via grep) |
| Lifecycle | Authored at P-VCD landing; updated when goldens are regenerated |

**Example shape (rv32x_dev)**:
```markdown
# Golden VCD regeneration — rv32x_dev

## Regeneration command

For each per-instruction scenario `<scenario>` under `tests/`:
```sh
riscv32-unknown-elf-gcc -march=rv32i -mabi=ilp32 -nostdlib \
    -T tests/link.ld tests/<scenario>.S -o build/<scenario>.elf
riscv32-unknown-elf-objcopy -O binary build/<scenario>.elf \
    build/<scenario>.bin
# … run hand-traced reference Python simulator:
python3 tb/ref_sim.py --image build/<scenario>.bin \
    --vcd golden/<scenario>.vcd --cycles <expected-count>
```

## External source

Reference is the hand-traced Python instruction-set simulator
`tb/ref_sim.py` cross-validated against the RISC-V ISA reference
manual (RV32I, unprivileged spec v2.2).

## Simulator + version

`ref_sim.py` is a hand-authored cycle-accurate model. Python
3.11.5. Not a Verilog simulator.

## Environment / dependencies

- riscv-tests binaries (`riscv32-unknown-elf-gcc`,
  `riscv32-unknown-elf-objcopy`) from the M7 dev container.
- Python 3.11+ stdlib only (no PyPI deps).
```

---

## 9. `SIGNAL_MAP.toml` entity (optional, per project)

| Property | Value |
|---|---|
| Owner | `test/audited/<project>/golden/SIGNAL_MAP.toml` (optional) |
| Format | TOML (parsed by Python stdlib `tomllib`, 3.11+) |
| Purpose | Alias upstream-NSL-toolchain-emitted signal names ↔ `nslc`-emitted signal names when they differ (rare for non-CPU projects; common for CPU projects with struct-field flattening differences) |
| Schema | Array of tables under `[[alias]]` with `golden = "..."` and `emitted = "..."` keys; both are dot-separated scope-path strings |
| Lifecycle | Authored on first audited-corpus-regression run when an unmapped signal mismatch surfaces; updated alongside `nslc` evolution if signal-name conventions shift |

**Example**:
```toml
[[alias]]
golden  = "top.cpu.regfile.r5"
emitted = "top.cpu.r_regfile_5"
[[alias]]
golden  = "top.cpu.alu.out"
emitted = "top.cpu.alu_out_w"
```

---

## 10. `tools/vcd_diff.py` entity

| Property | Value |
|---|---|
| Owner | `tools/vcd_diff.py` (NEW M7) |
| Language | Python 3.11+ stdlib only |
| LOC budget | ~150 (research.md §4) |
| CLI | `vcd_diff.py [--signal-map=<path>] <golden.vcd> <emitted.vcd>` |
| Exit codes | 0=equal, 1=divergence, 2=parse-error, 3=bad-CLI |
| Test peer | `tools/test_vcd_diff.py` (Python `unittest`, ~60 LOC) |

---

## 11. `check-audited` CMake target

| Property | Value |
|---|---|
| Owner | `cmake/audited_corpus.cmake` (NEW M7) under `test/audited/` |
| Type | Custom target (depends on `nslc` build product + the seven vendored projects) |
| Invocation | `cmake --build build --target check-audited` |
| Dependencies | `nslc` binary, the seven `test/audited/<project>/` directories, `tools/vcd_diff.py`, the M7 dev container (`:dev-m7`) with `iverilog` + `verilator` + `riscv-tests` |
| Per-cell shape | One lit-test instance per (project, simulator, scenario) tuple — expected ~14–30 cells total |
| Inclusion in `check` | Yes: top-level `check` target depends on `check-audited` (a non-zero exit fails CI) |

---

## 12. CI lint entity `check-audited-lint`

| Property | Value |
|---|---|
| Owner | `cmake/AuditedCorpusLint.cmake` |
| Invocation | Run automatically at `cmake -B build ...` configure time |
| Checks | (a) seven directories exist; (b) each has `PROVENANCE.md` with required `Key:` lines populated; (c) no `.gitmodules` references `test/audited/`; (d) no `CMakeLists.txt` under `test/audited/` invokes `FetchContent_Declare` or `ExternalProject_Add`; (e) no `golden/REGEN.md` contains `nslc` as a token |
| Failure mode | Configure fails with descriptive `message(FATAL_ERROR "...")` listing the failing project + reason |

---

## 13. Dev-container image entity (`:dev-m7`)

| Property | Value |
|---|---|
| Tag | `ghcr.io/koyamanX/nsl-nslc:dev-m7` |
| Parent | Built FROM `ghcr.io/koyamanX/nsl-nslc:dev` via `PARENT_IMAGE` build-arg |
| Additions over parent | `verilator` v5.024 (built from source, pinned by git tag) + `riscv-tests` binaries from upstream master SHA + `python3-toml` (NOT needed — stdlib `tomllib` suffices; verifying not included) |
| Rolling tag relationship | M7 PR uses `:dev-m7`; follow-on PR bumps `:dev` to the same image SHA |
| Lockfile | SHA recorded in `docker/publish-images.lockfile.yml` |

---

## 14. Pipeline IR data-flow (M7's complete view)

```text
Input: <input.nsl>
        │
        ▼
[ Preprocess ]   ← M1: PreprocessedSource (token stream + #line)
        │
        ▼
[ Lex ]          ← M1: token::Token stream
        │
        ▼
[ Parse ]        ← M2: ast::CompilationUnit
        │
        ▼
[ Sema ]         ← M3: SemaResult (symbol table + type system)
        │
        ▼
[ lowerToNSL ]   ← M5: mlir::OwningOpRef<mlir::ModuleOp>
        │           — IR contains nsl::* ops + (sometimes) un-expanded
        │             generate-loop placeholders
        │
        ▼
[ runNSLPasses ] ← M5: 6-slot structural-expansion pipeline
        │           — IR contains pure nsl::* ops (no generate-loop,
        │             no %IDENT% residue, etc.)
        │
        ▼
[ lowerToCIRCT ] ← M6: NSLToCIRCTPass (full-conversion mode)
        │           — IR contains hw/comb/seq/fsm/sv only;
        │             no nsl::* residue
        │
        ▼                                ┌────────────────────────────┐
[ runCIRCTPasses ] ← M7 NEW              │ THREE-PASS PIPELINE:       │
        │                                │   1. createConvertFSMToSVPass()
        │                                │      → fsm → seq + comb    │
        │                                │   2. createLowerSeqToSVPass()
        │                                │      → seq → sv.reg + sv.alwaysff
        │                                │   3. createPrepareForEmissionPass()
        │                                │      → emission-prep       │
        │                                └────────────────────────────┘
        │           — IR contains hw + comb + sv only
        │             (no fsm, no seq)
        │
        ▼
[ emit ]         ← M7 NEW
        │           — dispatches by -o shape:
        │             • directory → exportSplitVerilog (per-module .v)
        │             • file      → exportVerilog (single .v)
        │             • stdout    → exportVerilog (single .v)
        ▼
Output: Verilog bytes (1 or more .v files / stdout)
```

Diagnostics from any stage flow through a single `basic::DiagnosticEngine`
backed by the `DiagnosticBridge` RAII handler that bridges MLIR's
`mlir::ScopedDiagnosticHandler` into the project's diagnostic
channel. Result: end-to-end `nslc -emit=verilog` produces either a
clean Verilog file (or directory) on exit 0, or diagnostics on
exit 1, or a file-error report on exit 3. Nothing else.
