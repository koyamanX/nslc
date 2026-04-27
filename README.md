<!-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception -->

# nslc

A compiler for NSL (Next Synthesis Language), targeting synthesizable Verilog via LLVM, MLIR, and CIRCT.

`nslc` parses NSL source, lowers it through a dedicated MLIR dialect, then through CIRCT's `hw` / `comb` / `seq` / `fsm` / `sv` dialects, and finally emits Verilog using `ExportVerilog`. Every NSL abstraction — procedures, states, control terminals, structural-generate loops — is preserved as long as it is meaningful, and lowered only at the level where it matches a hardware primitive.

## About NSL

NSL is a compact hardware description language developed by Overtone Corporation, designed around message-driven semantics. It lets a designer describe substantial circuits (a small CPU fits in fewer than 50 lines) while remaining fully synthesizable. This compiler is an independent open-source implementation; it is not affiliated with Overtone Corporation. The language is documented in [`docs/spec/`](./docs/spec/), distilled from the public NSL Reference Manual and tutorials and cross-checked against ~12,000 lines of audited open-source NSL.

## Highlights

- **NSL → Verilog** through a multi-stage MLIR pipeline. Every stage is inspectable from the command line.
- **Built on CIRCT.** All the work below the `nsl` dialect — register inference, state-machine lowering, Verilog emission — is stock CIRCT. No hand-rolled netlist passes.
- **Source-locating diagnostics.** Every IR operation carries the originating NSL `SourceRange`; errors from any stage (parser, sema, MLIR pass, CIRCT pass) round-trip to a precise file:line:col.
- **Modern C++17 codebase.** Layered into nine static libraries with single public headers; the driver is ~60 lines.

## Status

`nslc` is under active development. The language specification and compiler/tooling design are stabilized in [`docs/`](./docs/); implementation work follows the layered milestone plan in [§Roadmap](#roadmap) below. Expect breaking changes to internal interfaces; the NSL-language surface itself is stable and tracks the published reference manual.

## Roadmap

The compiler is delivered as nine static C++ libraries in strict
dependency order (Constitution Principle II); tooling lands in
parallel after the sema layer is in place.
[`.specify/memory/constitution.md`](./.specify/memory/constitution.md)
governs *what* must be true; this section governs *when* the project
will achieve it.

**Notation.** `Mxx`..`Myy` are compiler-track milestones; `Txx`..`Tyy`
tooling-track. `P-*` are project-enablement deliverables —
`P-CI`/`P-VEN`/`P-VCD` gate compiler milestones (below);
`P-LIN`/`P-TS` are workflow-tier and live in
[`CONTRIBUTING.md`](./CONTRIBUTING.md) §3.8. "Test gate" is the
*minimum* evidence required to call a milestone done; Constitution
Principles V (determinism), VI (layered tests), VII (spec/design
coupling), VIII (TDD), and IX (CI green) apply unconditionally at
every milestone.

### Compiler track (Mxx–Myy)

| # | Deliverable (libraries land in this milestone) | Test gate | Constitutional anchor |
|---|---|---|---|
| **M0** | Build scaffolding: nine library CMake skeletons (`add_nsl_library`), lit + FileCheck wiring, `.clang-tidy` profile, SPDX-header presence script, smoke `nslc --version` binary. | CI pipeline green on smoke target; SPDX-header check passes; `nslc --version` runs. | II (layered structure); IX (CI online — see P-CI) |
| **M1** | `nsl-basic` (1) + `nsl-preprocess` (2) + `nsl-lex` (3). Source-locating diagnostic plumbing; `#line` survives the preprocessor/parser seam. | Lexer tests on token streams (Z/X/U digits, `_`-prefix, `%IDENT%` macros, all reserved keywords); preprocessor tests covering directive set; `#line` round-trip golden. | IV (diagnostics); VI (lexer + preprocessor tests) |
| **M2** | `nsl-parse` (4) + `nsl-ast` (5). | AST-snapshot tests covering every grammar production from `docs/spec/nsl_lang.ebnf §§1–11`; parser-note `N1`–`N14` disambiguation tests pass. | VI (parser tests); VII (spec coupling) |
| **M3** | `nsl-sema` (6); `SymbolTable` + `TypeSystem`. **This is the milestone the tooling track gates on (see [Tooling track](#tooling-track-txxtyy) below, T2 onward).** | **One pass-case + one fail-case test per `S1`–`S29` (with diagnostic-string assertion per Principle VIII rule for `Sn` constraints)**, all green. | VI (sema tests, NON-NEGOTIABLE); VIII (test-first); I (spec authority) |
| **M4** | `nsl-dialect` (7); `nsl-opt` round-trip operational. Every `nsl::*` op verifies. | `nsl-opt foo.mlir → verify → print → diff foo.expected.mlir` for every op listed in `nsl_compiler_design.md` §7. | VI (dialect tests); III (CIRCT alignment — no hand-rolled passes) |
| **M5** | `nsl-lower` part 1 (8a): AST → `nsl` dialect; structural-expansion passes (generate-loop unroll, struct-SSA-split, `%IDENT%` residue check). | FileCheck on `nslc -emit=mlir` for representative samples per AST node kind; determinism gate (byte-stable across two builds). | VI (lowering tests); V (determinism); III (`nsl` dialect is the seam) |
| **M6** | `nsl-lower` part 2 (8b): `nsl` → CIRCT (`hw`/`comb`/`seq`/`fsm`/`sv`). `nsl::ProcOp` / `nsl::StateOp` / `nsl::SeqOp` lower to `fsm::MachineOp`. | FileCheck on `nslc -emit=hw` for every per-op mapping in `nsl_compiler_design.md` §10; round-trip through stock CIRCT passes. | VI (lowering tests); III (stock CIRCT below dialect); V (determinism) |
| **M7** | `nsl-driver` (9); `nslc -emit=verilog` operational end-to-end. **`P-VEN` (vendoring) and `P-VCD` (golden VCDs) MUST land on or before M7 — see below.** | All seven audited projects (`cpu16`, `mips32_single_cycle`, `ahb_lite_nsl`, `mmcspi`, `SDRAM_Controler`, `rv32x_dev`, `turboV`) compile and simulate equivalently to their `golden/*.vcd` under Icarus and Verilator. | VI (end-to-end NON-NEGOTIABLE; "Delivery" vendoring; "Reference VCDs"); IX (no bypass); V (determinism) |
| **M8** | `riscv-formal` integration for `rv32x_dev`. Formal SUPPLEMENTS the golden VCD; it does not replace it (Principle VI). | `rv32x_dev` passes the riscv-formal ISA-compliance suite; result published in CI. | VI (formal clause); IX (CI integration) |
| **M9** | **1.0.0 release.** Tagged binaries + source tarball produced from CI per Principle IX. License audit complete; `PROVENANCE.md` per audited project verified. | Release pipeline green; LLVM-Exception scope verified; binary reproducibility check passes. | IX (release artifacts); Build/Code/Licensing |

> **M3 is the unlock point.** Hitting M3 enables the bulk of the
> tooling track (every milestone except T1) to start in parallel with
> subsequent compiler work.

### Compiler external dependencies (project-enablement)

The compiler track depends on three project-enablement deliverables.
Workflow-tier project-enablement (`P-LIN`, `P-TS`) is unrelated to the
compiler and lives in [`CONTRIBUTING.md`](./CONTRIBUTING.md) §3.8.

| # | Deliverable | Gates |
|---|---|---|
| **P-CI** | CI pipeline online with all six Principle IX stages (build matrix → static checks → unit/layer → lowering → e2e → formal-when-applicable). | M0 ideally; mandatory by first non-trivial PR (Principle IX) |
| **P-VEN** | Seven audited projects vendored under `test/audited/<project>/` with `PROVENANCE.md` (URL + SHA + license per project). | M7 (Principle VI "Delivery") |
| **P-VCD** | Golden VCDs committed at `test/audited/<project>/golden/<scenario>.vcd` with per-project `golden/REGEN.md`. | M7 (Principle VI "Reference VCDs") |

### Tooling track (Txx–Tyy)

Delivers `nsl-fmt`, `nsl-lsp`, `nsl-lint`, plus syntax-highlighter
grammars and editor packages. Implementation backed by the architecture
in
[`docs/design/nsl_tooling_design.md`](./docs/design/nsl_tooling_design.md).
All tooling binaries reuse `libNSLFrontend.a` (Constitution Principle
II) — no duplicated lexer/parser/sema.

| # | Deliverable | Test gate | Depends on |
|---|---|---|---|
| **T1** | TextMate grammar (`syntaxes/nsl.tmLanguage.json`) + `language-configuration.json`; GitHub `linguist` PR for `.nsl` recognition. Covers all reserved keywords from `docs/spec/nsl_lang.ebnf §15`. | TextMate scope tests on a fixture file matching every keyword, number form, and string literal; GitHub-linguist syntax-snapshot. | None — runs parallel from project start |
| **T2** | `nsl-fmt` v0: indent, brace style, operator spacing, NSL-specific rules (`nsl_tooling_design.md §5.3`). CLI + `--check` mode. | Round-trip test (format → format = noop); golden-output tests per NSL formatter rule. | M3 (Sema) |
| **T3** | `nsl-lsp` skeleton: `initialize`, `textDocument/didOpen`, `didChange`, `publishDiagnostics`, `foldingRange`. | LSP integration test: open a file with a Sema error, observe diagnostic; edit, observe re-diagnose. | M3 |
| **T4** | LSP feature batch (low-difficulty per `nsl_tooling_design.md §3.2`): `hover`, `definition`, `documentSymbol`, `semanticTokens`, `signatureHelp`. | One LSP-protocol test per method against a fixture document. | T3 |
| **T5** | LSP `formatting` + `rangeFormatting` (delegates to `nsl-fmt` library); CST-aware so it preserves `#line` directives. | LSP test: format request returns expected edits; range-format only edits inside the requested range. | T2, T3 |
| **T6** | `nsl-lint` framework + lint-rule registry; AST-local rules **W001–W005** and **S001–S005**. CLI + `--format=json` for CI consumption. | Per-rule fixture tests (one passing case + one failing case + suggested fix); JSON-output schema test. | M3 |
| **T7** | CFG/DFG analyses + hardware-design rules **H001–H009**. Sema-style rules requiring CFG (e.g., `S008` — `goto` across `seq` blocks) MAY land here. | Per-rule fixture tests; representative-design cases (e.g., `H002` combinational-loop on a known-bad fixture). | T6 |
| **T8** | Tree-sitter grammar (`grammar.js`) + highlight queries (`queries/highlights.scm`); VS Code extension shell consuming the WASM tree-sitter build. | Tree-sitter parse tree on the audited corpus matches expected structure (smoke); highlight-query golden test. | T1 (TextMate scope alignment) |
| **T9** | LSP feature batch (medium-difficulty): `completion`, `references`, `rename`, `codeAction`. Code-actions seed the fix-it database from `T6/T7` rule suggestions. | Per-method LSP test; rename across multi-file fixture; code-action applied → re-diagnose returns no error. | T4, T6 |
| **T10** | LSP power features: `inlayHint` (bit widths next to anonymous expressions), `prepareCallHierarchy`. | Inlay-hint correctness on width-inferring fixtures; call-hierarchy across `proc` invocations. | T4 |
| **T11** | Editor integrations: Neovim (`nvim-lspconfig`), Emacs (`lsp-mode` / `eglot`), Sublime (`LSP-nsl`), Sublime Syntax export. Per editor matrix in `nsl_tooling_design.md §4.4`. | Smoke install + open-a-file test per editor in CI (where feasible). | T3, T8 |
| **T12** | CI plumbing for tooling: `nsl-lint` GitHub Action; `nsl-fmt --check` pre-commit hook recipe; release pipeline for VS Code Marketplace. | Action passes on a fixture repo; pre-commit hook rejects unformatted commit. | T2, T6 |

> **Future tooling rules** (`S006`–`S010` AST-only rules; additional
> hardware rules) are not assigned a milestone here — they land
> incrementally inside the T6/T7 framework as needed. Adding a rule is
> not a milestone; it is a routine PR.

The inverse roll-up tables — *which milestone delivers NSL feature X /
`Sn` / `Nn` / `Pn`?* and *which milestone delivers LSP method / lint
rule / formatter capability / highlighter scope / editor X?* — live
in [`CLAUDE.md`](./CLAUDE.md) §1 and §2 respectively. Workflow
project-enablement (`P-LIN`, `P-TS`) and the rules for amending the
milestone plan live in [`CONTRIBUTING.md`](./CONTRIBUTING.md) §3.8 and §3.9.

## Usage

The driver runs the full NSL → Verilog pipeline by default and stops at any earlier stage on request:

```bash
nslc input.nsl -o output.v          # full pipeline (default)
nslc input.nsl -emit=tokens         # post-preprocess token stream
nslc input.nsl -emit=ast            # parsed AST
nslc input.nsl -emit=mlir           # nsl-dialect MLIR
nslc input.nsl -emit=hw             # CIRCT hw/comb/seq/sv form
nslc input.nsl -emit=verilog        # equivalent to default
```

Useful flags: `-I <dir>` for `#include` quote-form search paths; `-D NAME=value` for preprocessor defines; the `NSL_INCLUDE` environment variable for angle-form `#include` paths.

## Building

On a Linux x86_64 host with CMake ≥ 3.22, Ninja, GCC ≥ 9 (or Clang ≥ 10), Python ≥ 3.8, and a vendored prebuilt LLVM + MLIR + CIRCT install:

```bash
cmake -S . -B build -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DMLIR_DIR=/path/to/llvm-install/lib/cmake/mlir \
  -DCIRCT_DIR=/path/to/circt-install/lib/cmake/circt
cmake --build build
./build/bin/nslc --version          # smoke: prints `nslc <git-describe>`
ctest --test-dir build --output-on-failure
cd build && lit -v ../test
```

`scripts/ci.sh` is the single authoritative local-reproduction entry point — it runs the same six stages that GitHub Actions runs on every PR (Constitution Principle IX). Re-run any failing stage offline with `./scripts/ci.sh <stage>`. The full sequence is documented in [`specs/001-m0-build-ci-scaffolding/quickstart.md`](./specs/001-m0-build-ci-scaffolding/quickstart.md).

## Repository layout

> _Target layout for the first compiler release. Only `docs/`, `.specify/`, and `.claude/` are populated today; the implementation tree (`include/`, `lib/`, `tools/`, `test/`) is added milestone-by-milestone._

```
nslc/
├── docs/                       ← language spec + compiler/tooling design
│   ├── spec/                  (EBNF: preprocessor + NSL proper)
│   └── design/                (compiler & tooling architecture)
├── include/nsl/                ← public headers (one per library)
│   ├── Basic/  Preprocess/  Lex/  Parse/  AST/  Sema/
│   ├── Dialect/NSL/IR/  Lower/  Driver/
├── lib/                        ← library implementations
├── tools/
│   ├── nslc/                  (the compiler driver)
│   └── nsl-opt/               (mlir-opt for the nsl dialect)
├── test/                       ← lit + FileCheck regression tests
│   └── audited/                (vendored upstream NSL projects + PROVENANCE + golden VCDs)
├── CMakeLists.txt
├── LICENSE
└── README.md
```

## Documentation

All authoritative documentation lives in [`docs/`](./docs/) and is the single source of truth for both language and implementation:

| Path | Purpose |
|---|---|
| [`docs/CLAUDE.md`](./docs/CLAUDE.md) | Routing + editing guide for the `docs/` tree (humans + AI); §8 cross-reference table; §10 upstream NSL sources; §11 docs-specific PR checklist |
| [`docs/spec/nsl_pp.ebnf`](./docs/spec/nsl_pp.ebnf) | Preprocessor grammar |
| [`docs/spec/nsl_lang.ebnf`](./docs/spec/nsl_lang.ebnf) | NSL language grammar (incl. semantic constraints S1–S29 and parser notes N1–N14) |
| [`docs/design/nsl_compiler_design.md`](./docs/design/nsl_compiler_design.md) | Compiler architecture: pipeline, AST, dialect, lowering, diagnostics |
| [`docs/design/nsl_tooling_design.md`](./docs/design/nsl_tooling_design.md) | LSP, formatter, linter, and syntax highlighter design |
| [§Roadmap](#roadmap) below | Compiler `Mxx`–`Myy`, tooling `Txx`–`Tyy`, and the project-enablement deliverables (`P-CI`, `P-VEN`, `P-VCD`) that gate them |
| [`CLAUDE.md`](./CLAUDE.md) | Inverse lookup tables: which milestone delivers which NSL language feature / LSP method / lint rule / formatter capability / highlighter scope / editor |
| [`CONTRIBUTING.md` §3 Tooling and workflow](./CONTRIBUTING.md#3-tooling-and-workflow) | Operational playbook — Linear / Claude Code / Copilot / CodeRabbit roles, 6-phase workflow, application criteria by change size, MCP servers, workflow `P-*`, and rules for amending the milestone plan |

If `docs/spec/` and `docs/design/` ever disagree, treat it as a bug in `docs/design/` and report it — the spec wins.

## Companion tooling

Three binaries built on the same `libNSLFrontend.a` provide developer tooling — `nsl-lsp` (Language Server), `nsl-fmt` (formatter), `nsl-lint` (W/S/H lint tiers) — plus TextMate / tree-sitter grammars for editor integration. Delivery sequencing is in [§Roadmap](#roadmap) above (T-track); design in [`docs/design/nsl_tooling_design.md`](./docs/design/nsl_tooling_design.md).

## Contributing

See [`CONTRIBUTING.md`](./CONTRIBUTING.md) for the project-wide policy (sign-off, AI attribution, workflow phases, PR checklist) and [`docs/CLAUDE.md`](./docs/CLAUDE.md) for docs-specific rules (routing, editing protocol, §11 docs PR checklist).

## License

`nslc` is licensed under the **Apache License 2.0 with LLVM Exceptions**, the same license used by LLVM, MLIR, and CIRCT. See [`LICENSE`](./LICENSE) for the full text.

The LLVM Exception is significant for an HDL compiler: it makes explicit that Verilog or other artifacts produced *by* `nslc` are not encumbered by the compiler's own license. Whatever NSL you write and whatever Verilog you generate is yours.

## Acknowledgments

- **Overtone Corporation** for designing NSL and publishing the reference manual and tutorials.
- The maintainers of the open-source NSL projects (`rv32x_dev`, `turboV`, `mmcspi`, `SDRAM_Controler`, `mips32_single_cycle`, `ahb_lite_nsl`, `cpu16`) — their code was the empirical ground truth for resolving every ambiguity in the published spec.
- The **LLVM**, **MLIR**, and **CIRCT** communities for the infrastructure on which `nslc` is built.
