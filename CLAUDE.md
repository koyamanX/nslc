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

> **Status as of 2026-04-28**: M1 and M2 are delivered. The "M2"
> column entries below (covering `lang.ebnf §§1–11` parse + AST +
> N1/N2/N3/N5/N6/N7/N10/N11/N14 disambiguation + multi-error
> recovery) are all green: `nslc -emit=ast` works end-to-end;
> 169/169 ctest + 198/198 lit pass inside the dev container. M3
> (Sema for `Sn` constraints) and beyond remain forward-looking.

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
| Compilation unit + `struct` types | lang.ebnf §§1, 3 | M2; M3 (S18) | M4 (struct layout) | M6 |
| Top-level parameters | lang.ebnf §3.1; S16 | M2; M3 | M4 (param attrs) | M6 (param propagation) |
| `declare` block (ports, control terminals, modifiers) | lang.ebnf §4 | M2; M3 (S20 interface modifier) | M4 (`nsl::DeclareOp`) | M6 (HW ports) |
| `module` block | lang.ebnf §5 | M2; M3 | M4 (`nsl::ModuleOp`) | M6 (`hw::HWModuleOp`) |
| Internal-structure: `reg`, `wire`, `mem`, `proc_name`, `state_name` | lang.ebnf §6 | M2; M3 (S2, S6, S11) | M4 | M6 (`seq::CompRegOp`, `seq::FirMemOp`, `hw::WireOp`) |
| `func` / `proc` / `state` definitions | lang.ebnf §7 | M2; M3 (S6, S11, S21, S22, S26, S28) | M4 (`FuncInOp`, `FuncOutOp`, `ProcOp`, `StateOp`) | M6 (FSM lowering) |
| Action statements: `par` / `alt` / `any` | lang.ebnf §8; S13 | M2; M3 | M4 (`AltOp`, `AnyOp`) | M6 (`comb::MuxOp` chains) |
| Action statements: `seq` / `if` / `for` / `while` / `generate` | lang.ebnf §8; S7, S8, S9, S10 | M2; M3 | M4 (`SeqOp`); M5 (`generate` unroll pass) | M6 (`SeqOp` → `fsm::MachineOp` for in-`func`) |
| Atomic actions: transfers, control calls, `finish`, system tasks | lang.ebnf §9; S3, S12, S21 | M2; M3 | M4 (`TransferOp`) | M6 |
| System tasks: `_display`, `_finish`, `_init`, `_delay`, … | lang.ebnf §10; S17, S29 | M2; M3 | M4 (sim-only) | M6 (sim-only emit) |
| Expressions (sign-extend `#`, zero-extend `'`, slice, concat, conditional) | lang.ebnf §11; S14, S15 | M2 (incl. N5 `#` line-marker disambiguation); M3 | M5 | M6 (`comb::*`, `hwarith::*`) |
| Width / constant expressions | lang.ebnf §12 | M2; M3 | M5 | — |
| Semantic constraints `S1`–`S29` | lang.ebnf S1–S29 area | **M3 — one pass-case + one fail-case test each per Principle VI** | — | — |
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
**Active feature**: `006-m3-sema` — land the `nsl-sema` (6) static
library (SymbolTable + TypeSystem + per-`Sn` constraint checking
`S1`–`S29`), threading it into the driver between parse and any
`-emit=*` from `-emit=ast` forward. Hybrid recovery: one top-down
resolution pass + per-`Sn` independent walkers (per /speckit-
clarify Q3 Option C); paired pass+introspection for the 6
constructive `Sn` `S13`/`S18`/`S19`/`S23`/`S24`/`S27` (Q1 Option B);
`-emit=ast` re-cut in place to print resolved types and decl-loc
inline (Q2 Option A). For technologies, project structure, Symbol
+ Type entity catalog, contracts, and quickstart, read the current
plan: [`specs/006-m3-sema/plan.md`](./specs/006-m3-sema/plan.md).
Companion artifacts: [`spec.md`](./specs/006-m3-sema/spec.md),
[`research.md`](./specs/006-m3-sema/research.md),
[`data-model.md`](./specs/006-m3-sema/data-model.md),
[`contracts/`](./specs/006-m3-sema/contracts/),
[`quickstart.md`](./specs/006-m3-sema/quickstart.md).
<!-- SPECKIT END -->
