<!-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception -->

# Contract: `nslc -emit=verilog` driver flag

**Feature**: 011-m7-driver-e2e
**Owners**: `tools/nslc/main.cpp`, `include/nsl/Driver/EmitVerilog.h`,
`lib/Driver/EmitVerilog.cpp`, `lib/Driver/RunCIRCTPasses.cpp`,
`lib/Driver/Compilation.cpp`
**Status**: Frozen at M7
**Related FRs**: FR-001, FR-003, FR-004, FR-005, FR-006, FR-007,
FR-008
**Related contracts**: `circt-passes.contract.md` (the three stock-CIRCT
passes this flag chains); `driver-emit-hw.contract.md` from M6
(predecessor in the `-emit=*` ladder)

---

## §1 CLI surface

The `nslc` driver MUST accept `-emit=verilog` as a stage flag.
Effect: pipeline runs every M1–M6 stage plus M7's
`runCIRCTPasses → emit`, then halts.

Flag syntax:
- Long form: `-emit=verilog` (the only accepted form; the M5/M6
  precedent is `-emit=<stage>` with no whitespace around the `=`).
- No alias (M6 introduced an `-emit=circt` alias for `-emit=hw`;
  M7 introduces no alias for `-emit=verilog`).

Behavior matrix (governed by Q1 → B; full dispatch table in
`research.md` §2):

| `-o` argument | Filesystem state | Action |
|---|---|---|
| omitted | (irrelevant) | Single-file Verilog to stdout via `circt::exportVerilog(module, llvm::outs())` |
| `-o -` | (irrelevant) | Single-file Verilog to stdout (same as omitted) |
| `-o <path>/` (trailing slash) | (irrelevant) | Split-file via `circt::exportSplitVerilog(module, <path>)`; `mkdir -p <path>` if needed |
| `-o <path>` (no slash) | path exists, is a directory | Split-file (path treated as directory) |
| `-o <path>` (no slash) | path does not exist | Single-file via `circt::exportVerilog(module, raw_fd_ostream(<path>))` |
| `-o <path>` (no slash) | path exists, is a regular file | Single-file (overwrites the existing file) |
| `-o <path>` (no slash) | path exists, is a symlink to a directory | Split-file (symlink-target-is-directory case) |

Other flags inherited from the M2/M5/M6 `-emit=*` family:
`--diagnostic-format=text|json`, `-I<path>` (preprocessor include
search path), `-D<NAME>=<value>` (preprocessor macro define),
`--include-path=<path>`. M7 introduces no NEW CLI flags beyond
`-emit=verilog` itself.

---

## §2 Exit codes

| Exit code | Meaning |
|---|---|
| 0 | Success: Verilog bytes written to the resolved sink |
| 1 | At least one error-severity diagnostic at any pipeline stage (preprocess, lex, parse, sema, lowerToNSL, runNSLPasses, lowerToCIRCT, runCIRCTPasses, emit) |
| 3 | Input file could not be opened |
| 4 | Output sink could not be created (e.g., `-o <dir>/` and `mkdir -p` failed because of permissions) — NEW at M7 |

Matches the M6 `driver-emit-hw.contract.md` §3 baseline (codes
0/1/3) with one addition (exit 4) reflecting the new split-file
output sink failure mode. No other exit codes are defined.

---

## §3 Output buffering rule

**No partial output on error.** Verilog bytes are buffered in
memory (`std::string`) — or, in split-file mode, in a per-module
buffer dictionary keyed by module name — until completion. On a
diagnostic-bearing run, nothing is written to the resolved sink.

This matches the M2/M5/M6 `EmitAST` / `EmitMLIR` / `EmitHW`
convention. In split-file mode, the implementation MAY use a
temp directory + atomic rename for the final sink update (left as
implementation detail; the contract surface is "nothing-or-everything"
not "how").

---

## §4 Determinism contract

Two invocations of `nslc -emit=verilog <input> -o <output>` with
identical:
- input bytes,
- CLI flags,
- environment variables (LANG, LC_*, NSL_INCLUDE),
- working directory,
- toolchain version (compiler + LLVM + CIRCT pin),

MUST produce byte-identical `<output>`. In split-file mode, the
set of files written MUST be identical (same filenames, same
counts, byte-identical contents).

Implementation rules to preserve determinism:
- `mlir::PassManager` SHALL NOT enable parallel mode.
- All MLIR walks SHALL be deterministic-order (pre-order DFS;
  no `std::unordered_*` based iteration of ops).
- `circt::exportVerilog` is byte-deterministic by upstream
  contract (research.md §3); the driver MUST NOT inject any
  per-run state (no `std::chrono` reads during emit; no env-var
  reads during emit).

Test gate: `test/Driver/emit_verilog/determinism.test` runs the
flag twice with a fixed `.nsl` input and `diff -q`s the outputs.
The two-host-path CI extension from M5/M6 covers cross-runner
byte stability.

---

## §5 Diagnostic routing

Every diagnostic produced anywhere in the `nslc -emit=verilog`
pipeline MUST route through the project's single
`basic::DiagnosticEngine`. Specifically:

- M1–M6 stage diagnostics flow through the existing channels
  unchanged (the M5 `DiagnosticBridge` RAII handler already
  covers `lowerToCIRCT`).
- M7's `runCIRCTPasses` registers a `mlir::ScopedDiagnosticHandler`
  for the pass-manager run; the handler routes
  `mlir::emitError`/`mlir::emitWarning`/`mlir::emitRemark` into
  `basic::DiagnosticEngine` with `mlir::Location → SourceLocation`
  preservation.
- M7's `emit` registers (or reuses) the same handler for the
  ExportVerilog invocation. ExportVerilog's diagnostic-bearing
  failures (e.g., illegal module name, port-name collision) are
  surfaced as `basic::Diagnostic` instances.

No diagnostic SHALL escape to a second stderr channel. CI grep
guard: `scripts/ci.sh` extends the M5/M6 diag-double-channel grep
to cover `lib/Driver/EmitVerilog.cpp` + `lib/Driver/RunCIRCTPasses.cpp`.

---

## §6 Build / link contract

`lib/Driver/CMakeLists.txt` adds:
- Source: `EmitVerilog.cpp`, `RunCIRCTPasses.cpp`.
- Header: `${CMAKE_SOURCE_DIR}/include/nsl/Driver/EmitVerilog.h`.
- `LINK_LIBS` additions (NEW M7):
  - `CIRCTExportVerilog`
  - `CIRCTSeqTransforms`
  - `CIRCTSVTransforms`
  - `CIRCTFSMTransforms`
- `target_compile_options(nsl-driver PRIVATE -fno-rtti)` unchanged
  (already in place from M5/M6).

The four CIRCT libs are vendored CIRCT's standard `CIRCTConfig.cmake`
exports — no project-side fork. No additional include-dir
configuration beyond what `find_package(CIRCT REQUIRED CONFIG)`
already provides at the top-level CMakeLists.

---

## §7 Test surface

Required lit fixtures under `test/Driver/emit_verilog/`:

| Fixture | Purpose | Asserts |
|---|---|---|
| `single_module.test` | Keystone path | One module → one `always_ff` + one `assign`; exit 0 |
| `multi_module_dir.test` | Q1 → B path (a) | `-o <dir>/` produces per-module `.v` files in directory |
| `multi_module_file.test` | Q1 → B path (b) | `-o <file>` produces single combined `.v` |
| `multi_module_stdout.test` | Q1 → B path (c) | `-o -` (or omitted) emits to stdout |
| `determinism.test` | §4 | Two runs `diff -q` — zero exit |
| `sema_error.test` | §5 | Sema error blocks emit; exit 1; no `.v` written |
| `iverilog_smoke.test` | Story 1 acceptance #3 | Emitted `.v` accepted by `iverilog -g2012`; exit 0 |
| `verilator_smoke.test` | Story 1 acceptance #3 | Emitted `.v` accepted by `verilator --lint-only`; exit 0 |

Fixtures land in TDD order: failing fixture first, implementation
makes it green (Constitution Principle VIII).

**Amendment 2026-05-12 (M7 Phase 7 cleanup)**: original contract
listed 10 fixtures including `passes_failure.test` + `export_failure.test`
for the §5 diagnostic-routing rule. Those two fixtures landed as
TODO-placeholders with `XFAIL: *` + a literal "TODO" CHECK string
(vacuous per Constitution Principle VIII line 416-421 — a test
that "passes" by failing for an unrelated reason). Removed in
`m7: phase 7 audit cleanup` commit. The §5 diagnostic-routing rule
remains in force; future failure-inducing inputs (e.g., an FSM
with no states that trips the FSM verifier post-`runCIRCTPasses`)
should land as real-failure-mode fixtures via `/nsl-test-author`
when such inputs surface organically during M-track development.

---

## §8 Forward compatibility

Future post-M7 enhancements anticipated:

- M8 may add a `--formal=riscv` flag that diverts the pipeline to
  riscv-formal Yosys frontend instead of `exportVerilog`. This
  contract anticipates that flag by NOT enumerating `-emit=verilog`
  as the only post-runCIRCTPasses sink (the post-passes `emit`
  member function is the dispatch point).
- M9 may add a `--reproducible` flag pinning environment for
  release-artifact reproducibility. This contract anticipates that
  flag by encoding the implicit determinism guarantee in §4 (so
  the flag would be a no-op for the default code path; it only
  affects auxiliary outputs like dependency tracking).

No anticipated change SHALL retroactively modify §1–§7. Any
backwards-incompatible change requires a /speckit-clarify session
+ contract amendment in a fresh feature spec.
