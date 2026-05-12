<!-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception -->

# CLAUDE.md — Project conventions for Claude Code

This file holds the project-root milestone **lookup tables** (NSL
language feature → milestone, tooling feature → milestone). The
forward `Mxx`–`Myy` and `Txx`–`Tyy` tables themselves live in
[`README.md`](./README.md) §Roadmap. Documentation routing inside
`docs/` is in [`docs/CLAUDE.md`](./docs/CLAUDE.md).

---

## 1. Language-feature roll-up (NSL spec → milestone)

Inverse view of the compiler-track table in
[`README.md`](./README.md) §Roadmap: given an NSL grammar area, `Sn`,
`Nn`, or `Pn`, this table tells you the milestone(s) that deliver its
support.

References to `lang.ebnf §X` are sections in
[`docs/spec/nsl_lang.ebnf`](./docs/spec/nsl_lang.ebnf); references to
`pp.ebnf §X` are sections in
[`docs/spec/nsl_pp.ebnf`](./docs/spec/nsl_pp.ebnf).

> **Status as of 2026-05-04**: M1, M2, M3, M4, M5, and M6 (this
> branch, structurally feature-complete) are delivered. The "M5 (...)"
> column entries — `%IDENT%` residue check (`NSLCheckSemanticsPass`),
> `generate` unroll (`NSLExpandGeneratePass`), expression visitor
> coverage, width / constant expressions (`NSLResolveParamsPass`) —
> are all wired into the `Compilation::runNSLPasses` pipeline (slots
> 1, 2, 6 of 6 per `008-m5-structural-passes/contracts/pass-pipeline.
> contract.md` §2). The "M6 ✓" entries — the `nsl::*` → CIRCT
> conversion pass (`NSLToCIRCTPass`) producing `hw`/`comb`/`seq`/
> `fsm`/`sv` IR per `010-m6-circt-lowering/contracts/circt-lowering.
> contract.md` §1 — are wired into `Compilation::lowerToCIRCT` and
> exposed via `nslc -emit=hw`. 620 PASS + 3 XFAIL out of 623 lit
> tests inside the dev container. M7 (`-emit=verilog` via stock
> CIRCT passes + `circt::ExportVerilog`) and beyond remain
> forward-looking.

| Language area | Spec reference | Lex / parse / sema | Lower to dialect | Lower to CIRCT |
|---|---|---|---|---|
| Lexical: identifiers, numbers (Z/X/U), strings, `_`-prefix names | lang.ebnf §13; N11 | M1 (lex) | — | — |
| Whitespace, comments | lang.ebnf §14 | M1 | — | — |
| Reserved keywords | lang.ebnf §15 (783–824) | M1 (recognition); T1+T8 (highlighter) | — | — |
| Preprocessor: `#include` (quote/angle) | pp.ebnf §2.1; P8 | M1 | — | — |
| Preprocessor: `#define`/`#undef` (incl. compile-time math) | pp.ebnf §2.2; P5/P7 | M1 | — | — |
| Preprocessor: `#ifdef`/`#ifndef`/`#if`/`#else`/`#endif` | pp.ebnf §2.3 | M1 | — | — |
| Preprocessor: `#line` (crosses the seam) | pp.ebnf §2.4; P13; lang.ebnf N14 | M1 (emit); M2 (consume); preserved through every later stage per Principle IV | — | — |
| Preprocessor: `%IDENT%` macro splicing | pp.ebnf §4; P3 | M1 | M5 (residue-free check) | — |
| Preprocessor: bare-macro textual substitution + recursion bound | pp.ebnf P10 (amended in 003-macro-textual-concat) | M1 (`MacroExpander` pre-pass; 256-level cycle bound; FR-007 locked diagnostic) | — | — |
| Compile-time helpers `_int`/`_pow`/`_sin`/… | pp.ebnf §3 | M1 (preprocessor parse + eval per pp.ebnf P5/P6/P7/P12); M3 separately delivers the NSL-language Sema constant evaluator for `Sn` constraints (S15 bit-slice indices etc.) — that's a different evaluator from the preprocessor's | — | — |
| Compilation unit + `struct` types | lang.ebnf §§1, 3 | M2; M3 (S18) | M4 (struct layout) | M6 ✓ |
| Top-level parameters | lang.ebnf §3.1; S16 | M2; M3 | M4 (param attrs) | M6 ✓ (param propagation; ParamPatterns) |
| `declare` block (ports, control terminals, modifiers) | lang.ebnf §4 | M2; M3 (S20 interface modifier) | M4 (`nsl::DeclareOp` + `nsl::InputPortOp` / `nsl::OutputPortOp` / `nsl::InoutPortOp`; post-merge amendment 2026-05-05 #9; field-level amendment 2026-05-05 #10 surfaces S20 as `interface_clock` + `interface_reset` `OptionalAttr<StrAttr>` on `nsl::DeclareOp`) + M5 visitor (`visit(DeclareBlock)`; populates the attrs from `ast::DeclareBlock::clockName()` / `resetName()`) | M6 ✓ (HW ports; user-named clock + reset on the explicit-`interface` path per amendment-#10; ModulePatterns) |
| `module` block | lang.ebnf §5 | M2; M3 | M4 (`nsl::ModuleOp`) | M6 ✓ (`hw::HWModuleOp`) |
| Internal-structure: `reg`, `wire`, `mem`, `proc_name`, `state_name` | lang.ebnf §6 | M2; M3 (S2, S6, S11) | M4 | M6 ✓ (`seq::FirRegOp` default / `seq::CompRegOp` on S20 interface path, `seq::FirMemOp`, `hw::WireOp`) |
| `func` / `proc` / `state` definitions | lang.ebnf §7 | M2; M3 (S6, S11, S21, S22, S26, S28) | M4 (`FuncInOp`, `FuncOutOp`, `ProcOp`, `StateOp`) | M6 ✓ (FSM lowering — FSMPatterns) |
| Action statements: `par` / `alt` / `any` | lang.ebnf §8; S13 | M2; M3 | M4 (`AltOp`, `AnyOp`) | M6 ✓ (`comb::MuxOp` chains; ControlPatterns) |
| Action statements: `seq` / `if` / `for` / `while` / `generate` | lang.ebnf §8; S7, S8, S9, S10 | M2; M3 | M4 (`SeqOp`); M5 (`generate` unroll pass) | M6 ✓ (`SeqOp` → `fsm::MachineOp` for in-`func`; `IfOp` → `comb::MuxOp` per Q3 → A) |
| Atomic actions: transfers, control calls, `finish`, system tasks | lang.ebnf §9; S3, S12, S21 | M2; M3 | M4 (`TransferOp`) | M6 ✓ |
| System tasks: `_display`, `_finish`, `_init`, `_delay`, … | lang.ebnf §10; S17, S29 | M2; M3 | M4 (sim-only) | M6 ✓ (sim-only emit; SimPatterns under `sv.ifdef "SIMULATION"`) |
| Expressions (sign-extend `#`, zero-extend `'`, slice, concat, conditional) | lang.ebnf §11; S14, S15 | M2 (incl. N5 `#` line-marker disambiguation); M3 | M5 | M6 ✓ (`comb::*` only — Q1 → A; ArithPatterns + BitOpPatterns) |
| Width / constant expressions | lang.ebnf §12 | M2; M3 | M5 | — |
| Semantic constraints `S1`–`S29` | lang.ebnf S1–S29 area | **M3 — one pass-case + one fail-case test each per Principle VI**[^constructive] | — | — |

[^constructive]: 23 of the 29 `Sn` are error/warning constraints checked via the standard pass+fail lit fixture pair under `test/sema/s<NN>/`. The remaining 6 — `S13`, `S18`, `S19`, `S23`, `S24`, `S27` — are **constructive** (per Clarifications session 2026-04-28 Q1 → Option B; constitution v1.6.0 Principle VIII carve-out): they emit no diagnostic and instead populate an introspection observable on the symbol/type system. Their fail-case shape is a paired GoogleTest assertion under `test_unit/constructive_sn_test/s<NN>_test.cc` that asserts the *opposite* observable via `EXPECT_NONFATAL_FAILURE`. See `specs/006-m3-sema/contracts/sema-api.contract.md` Invariant 4 for the full introspection-API table.
| Parser notes `N1`–`N14` | lang.ebnf N1–N14 area | M2 (most); M3 (S/N interactions) | — | — |
| Preprocessor notes `P1`–`P13` | pp.ebnf P1–P13 area | M1 (one test each per Principle VI) | — | — |

> **Audit hook.** When a new `Sn`, `Nn`, or `Pn` is added to the spec
> (Principle I monotonic-numbering), this table MUST gain a row in the
> same change. If not, the change violates Principle VII (spec/design
> coupling).

---

## 2. Tooling-feature roll-up

Inverse view of the tooling-track table in
[`README.md`](./README.md) §Roadmap: given a specific tool feature
(LSP method, lint rule, formatter capability, highlighter scope,
editor integration), this section tells you when it lands.

### 2.1 LSP methods (per `nsl_tooling_design.md §3.2`)

| Method | Difficulty | Milestone |
|---|---|---|
| `publishDiagnostics` (errors / warnings) | Trivial | T3 |
| `textDocument/foldingRange` | Trivial | T3 |
| `textDocument/hover` | Low | T4 |
| `textDocument/definition` | Low | T4 |
| `textDocument/documentSymbol` (outline) | Low | T4 |
| `textDocument/semanticTokens` | Low | T4 |
| `textDocument/signatureHelp` | Low | T4 |
| `textDocument/formatting` | Low | T5 |
| `textDocument/rangeFormatting` | Low | T5 |
| `textDocument/references` | Medium | T9 |
| `textDocument/completion` | Medium | T9 |
| `textDocument/rename` | Medium | T9 |
| `textDocument/codeAction` (quick fix) | Medium–High | T9 |
| `textDocument/inlayHint` | Medium | T10 |
| `textDocument/prepareCallHierarchy` | Medium | T10 |

### 2.2 Lint rules (per `nsl_tooling_design.md §6.1`)

| Tier | Rule | Subject | Milestone |
|---|---|---|---|
| W (grammar-level warnings) | W001 | unreachable `state` after `goto` | T6 |
|   | W002 | unused `reg`/`wire`/`mem` declaration | T6 |
|   | W003 | `function` keyword used (prefer `func`; S26) | T6 |
|   | W004 | `label` used as identifier (N10) | T6 |
|   | W005 | shadowed identifier | T6 |
| S (semantic / style) | S001 | bit-width mismatch in auto-widened assignment | T6 |
|   | S002 | `alt` block with only one case | T6 |
|   | S003 | `any` block where `alt` was meant | T6 |
|   | S004 | unreachable state | T6 |
|   | S005 | `proc` never invoked | T6 |
|   | S006–S010 | rules requiring shadowing / type / coverage / const-eval | T6 framework, lands incrementally |
|   | S008 | `goto` across `seq` blocks (CFG-required) | T7 |
| H (hardware-design) | H001 | missing reset path | T7 |
|   | H002 | combinational loop through `wire` | T7 |
|   | H003 | multi-driver on output | T7 |
|   | H004 | unregistered output (glitch risk) | T7 |
|   | H005 | state machine deadlock (no transitions out) | T7 |
|   | H006 | mem depth > 4096 (BRAM-inference risk) | T7 |
|   | H007 | mem read-during-write hazard | T7 |
|   | H008 | async reset without synchronizer | T7 |
|   | H009 | `func_in` mixed comb/seq invocation | T7 |

> **Adding a new lint rule (W/S/H) post-T7 is a routine PR**, not a
> milestone. It must satisfy Principle VIII (TDD: per-rule fixture
> tests written first, observed failing) and update this table in the
> same change.

### 2.3 Formatter (per `nsl_tooling_design.md §5`)

| Capability | Milestone |
|---|---|
| Formatter v0: indent, brace style, operator spacing, NSL-specific rules (`§5.3`) + CLI + `--check` | T2 |
| LSP `formatting` / `rangeFormatting` integration | T5 |
| Pre-commit hook recipe | T12 |

### 2.4 Syntax highlighting (per `nsl_tooling_design.md §4`)

| Capability | Milestone |
|---|---|
| TextMate grammar + scope set per `§4.1`; `language-configuration.json` | T1 |
| Tree-sitter grammar + highlight queries; VS Code WASM consumer | T8 |
| Sublime Syntax export | T11 |

### 2.5 Editor integrations (per `nsl_tooling_design.md §4.4`)

| Editor | TextMate (T1) | Tree-sitter (T8) | LSP (T3+) | Packaging milestone |
|---|---|---|---|---|
| VS Code | T1 | T8 | T3 onward | T8 / T11 |
| Neovim | T1 (vim syntax) | T8 (native) | T3 onward | T11 |
| Emacs | T1 (`nsl-mode.el`) | T8 (`tree-sitter.el`) | T3 onward | T11 |
| Sublime Text 4 | T1 | (via LSP) | T3 onward | T11 |
| GitHub | T1 only | — | — | T1 |
| JetBrains | (custom plugin out of scope) | — | T3 onward | T11 |

---

<!-- SPECKIT START -->
**Active feature**: `011-m7-driver-e2e` — M7 *demonstration
moment* (planning phase complete 2026-05-11; implementation phase
opens with /speckit-tasks). M7 delivers four orthogonal
sub-deliverables converging on a single Constitution Principle VI
NON-NEGOTIABLE acceptance gate (the audited-corpus regression):
(1) **`nsl-driver` end-to-end (layer 9 close-out)** — new
`nslc -emit=verilog` flag chains M6's CIRCT IR through three
stock CIRCT passes (`createConvertFSMToSVPass` →
`createLowerSeqToSVPass` → `createPrepareForEmissionPass`) into
`circt::exportVerilog` / `circt::exportSplitVerilog`. New
public header `EmitVerilog.h` mirroring M6's `EmitHW.h`; new
`Compilation::runCIRCTPasses` + `Compilation::emit` bodies.
Four new CIRCT `LINK_LIBS` entries: `CIRCTExportVerilog`,
`CIRCTSeqTransforms`, `CIRCTSVTransforms`, `CIRCTFSMTransforms`.
(2) **P-VEN (vendoring)** — seven audited NSL projects
(`cpu16`, `mips32_single_cycle`, `ahb_lite_nsl`, `mmcspi`,
`SDRAM_Controler`, `rv32x_dev`, `turboV`) copied verbatim under
`test/audited/<project>/` with `PROVENANCE.md` (URL + SHA +
License + Vendored-At). No submodules, no FetchContent.
(3) **P-VCD (golden VCDs)** — externally sourced VCDs at
`golden/<scenario>.vcd` with `REGEN.md` per project; no
self-referential goldens (CI lint blocks `nslc` invocations in
REGEN.md). (4) **Audited-corpus regression** — `cmake --build
build --target check-audited` runs 14 cells (7 projects × 2
simulators: Icarus + Verilator), each compiling+simulating
emitted Verilog against the project's testbench and comparing
the resulting VCD to the golden via the vendored
`tools/vcd_diff.py` (Python 3.11+ stdlib-only semantic-equal
comparator; ignores `$date`/`$version`/`$timescale`/`$comment`;
intersects signal sets with optional per-project
`SIGNAL_MAP.toml` aliasing). Three /speckit-clarify decisions
pinned conventions: Q1 → B hybrid `-o` dispatch (directory ⇒
split-file via `exportSplitVerilog`; regular file ⇒ single
combined via `exportVerilog`; stdout/omitted ⇒ single combined);
Q2 → B `tools/vcd_diff.py` semantic-equal with optional
`SIGNAL_MAP.toml` aliasing; Q3 → A extend `Dockerfile.dev` with
Verilator v5.024 + `riscv-tests` binaries via the established
`PARENT_IMAGE` build-arg pattern (`project_publish_images_buildx_isolation.md`).
New container tag `ghcr.io/koyamanX/nsl-nslc:dev-m7` —
non-rolling for M7 PR's review cycle; follow-on PR bumps `:dev`
post-merge. Wall-clock budget for `check-audited`: ≤ 15 min
on a standard CI runner. Two-simulator parity rule: a cell
PASSes only if BOTH simulators PASS — no per-simulator XFAILs.
Adding an 8th project post-M7 is a routine vendoring-only PR
with zero infra edits (auto-discovery via directory glob). For
technologies, project structure, entity catalog, contracts,
and quickstart, read the current plan:
[`specs/011-m7-driver-e2e/plan.md`](./specs/011-m7-driver-e2e/plan.md).
Companion artifacts:
[`spec.md`](./specs/011-m7-driver-e2e/spec.md),
[`research.md`](./specs/011-m7-driver-e2e/research.md),
[`data-model.md`](./specs/011-m7-driver-e2e/data-model.md),
[`contracts/`](./specs/011-m7-driver-e2e/contracts/),
[`quickstart.md`](./specs/011-m7-driver-e2e/quickstart.md).
<!-- SPECKIT END -->
