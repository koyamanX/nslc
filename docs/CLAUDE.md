<!-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception -->

# CLAUDE.md — Guide for the `docs/` tree (humans and AI agents)

This `docs/` directory is the **single source of truth** for the NSL (Next Synthesis Language) language specification and tooling design within the **nslc** compiler project. Use this file to find what to read; do **not** read whole files unless explicitly needed.

This file consolidates what was previously split across `docs/README.md` (human-facing intro) and `docs/CONTRIBUTING.md` (docs-specific contribution rules). For project-wide contribution policy — licensing, `Signed-off-by`, AI attribution (`Assisted-by`), commit-message style — see [`../CONTRIBUTING.md`](../CONTRIBUTING.md). The rules in §11 below are *additional* requirements that apply when you touch `docs/`.

> Reading whole files here costs ~40k tokens. Section TOCs below let you `view` with `view_range=[start,end]` and stay under ~2k tokens per lookup.

---

## 1. Directory layout

This directory sits inside the nslc compiler project:

```
nslc/                              ← parent project (compiler implementation)
├── CONTRIBUTING.md                ← contribution guide (incl. AI assistant policy)
└── docs/                          ← you are here
    ├── CLAUDE.md                  ← this file
    ├── README.md                  ← human-facing intro
    ├── spec/                      ← authoritative grammar (input to parser/lexer)
    │   ├── nsl_pp.ebnf           (559 lines) — preprocessor grammar
    │   └── nsl_lang.ebnf         (1155 lines) — NSL language proper
    ├── design/                    ← implementation specifications (how we implement the spec)
    │   ├── nsl_compiler_design.md (1337 lines) — frontend → MLIR → CIRCT → Verilog
    │   └── nsl_tooling_design.md  (1015 lines) — LSP, formatter, linter, highlighter
```

The project roadmap lives in the project root (orthogonal to spec/design):
- compiler `Mxx`–`Myy` + tooling `Txx`–`Tyy` + compiler `P-*` (`P-CI`, `P-VEN`, `P-VCD`) → [`../README.md`](../README.md) §Roadmap
- NSL language-feature → milestone and tooling-feature → milestone roll-ups → [`../CLAUDE.md`](../CLAUDE.md) §1–§2
- Workflow `P-*` (`P-LIN`, `P-TS`) and milestone-plan amendment rules → [`../CONTRIBUTING.md`](../CONTRIBUTING.md) §3.8–§3.9

Every file carries an `SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception` header on its first line; the line tables below already account for it.

The two `docs/` subdirectories answer different questions:
- `spec/*.ebnf` — what the language **is**.
- `design/*.md` — how the implementation **handles it**.

The project-root milestone plan (above) answers **when** each piece lands.

When in doubt about *what the language is*, **the spec wins**. If `design/` and `spec/` disagree, treat it as a bug in `design/` and report it. If the milestone plan and a design doc disagree on *delivery sequencing*, the milestone plan wins; if they disagree on *design*, the design doc wins.

---

## 2. Reading strategy (read this before opening any file)

1. **Find your task** in §3 below. It tells you which sections to read.
2. **Use `view` with `view_range`** based on the line tables in §4–§8. Never `cat` an entire file.
3. **Cross-references** in §9 tell you when a topic spans both spec and design.
4. If you change `spec/`, you almost certainly need to update `design/` too — see §10.

Rule of thumb: a typical task needs **2–4 sections totaling 200–600 lines**, not a whole file.

**Human readers** wanting an overview before diving in: start with [`design/nsl_compiler_design.md`](./design/nsl_compiler_design.md) §2 (the end-to-end pipeline diagram, lines 18–126), then jump to whichever layer you care about via §3 below.

---

## 3. Task → section map

### Implementing the lexer
- `spec/nsl_lang.ebnf` lines **714–824** (lexical elements + reserved keywords)
- `spec/nsl_pp.ebnf` lines **345–390** (preprocessor lexical elements)
- `design/nsl_compiler_design.md` lines **132–148** (where the lexer fits in the layered architecture)
- Parser note **N5** (lang.ebnf 1035–1049) — `#` token disambiguation
- Parser note **N11** (lang.ebnf 1083–1105) — `_`-prefixed names

### Implementing the preprocessor
- `spec/nsl_pp.ebnf` **whole file is the spec** — but read in this order:
  - 15–64 (role + notation)
  - 67–90 (top-level structure)
  - 92–234 (directives: `#include`, `#define`, `#ifdef`, `#line`)
  - 236–310 (compile-time math sub-grammar)
  - 312–343 (`%IDENT%` macro references)
  - 391–559 (preprocessor notes P1–P13 — semantics not in the EBNF)
- `design/nsl_compiler_design.md` lines **132–148, 1294–1351** (driver wires the preprocessor)

### Implementing the parser
- `spec/nsl_lang.ebnf` lines **65–712** (the grammar productions)
- `spec/nsl_lang.ebnf` lines **1017–1155** (parser notes N1–N14 — disambiguation rules)
- `design/nsl_compiler_design.md` lines **189–197** (Parser class skeleton)
- `design/nsl_compiler_design.md` lines **299–682** (AST node hierarchy the parser builds)

### Implementing semantic analysis (Sema)
- `spec/nsl_lang.ebnf` lines **826–1015** (semantic constraints S1–S29 — the entire Sema spec)
- `design/nsl_compiler_design.md` lines **688–877** (SymbolTable + TypeSystem)
- `design/nsl_compiler_design.md` lines **1466–1479** (testing strategy: one test per S1–S29)

### Adding an MLIR `nsl` dialect op
- `design/nsl_compiler_design.md` lines **878–1135** (§7 dialect overview, op summary incl. M4 marker / lowering-helper consolidation, TableGen skeleton)
- `design/nsl_compiler_design.md` lines **1136–1217** (§8 AST → nsl-dialect lowering rules)
- `design/nsl_compiler_design.md` lines **1235–1305** (§10 nsl → CIRCT lowering — your op needs a target)
- **M6 contracts (post-2026-05-04)**: a new op now requires
  BOTH an M4 dialect contract amendment AND an M6 conversion
  pattern. The contracts that gate the latter:
  - `specs/010-m6-circt-lowering/contracts/circt-lowering.contract.md`
    §1 (per-op mapping freeze; bijection rule §2 — every row
    needs one `OpConversionPattern<T>` + one fixture pair).
  - `specs/010-m6-circt-lowering/contracts/lower-api.contract.md`
    §2 (10-symbol public surface of `Lower.h`; growing past 10
    is itself a contract amendment).
  - For `nsl.reg` reset/clock conventions on the new op (if any
    storage-bearing): `specs/010-m6-circt-lowering/contracts/firreg-convention.contract.md`.

### Writing a structural-expansion pass (generate-loop unroll, etc.)
- `design/nsl_compiler_design.md` lines **1204–1219** (§9 pass list)
- `spec/nsl_lang.ebnf` — search for `generate` / structural-expansion clauses (§§ around 8)
- M5 contract: `specs/008-m5-structural-passes/contracts/pass-pipeline.contract.md` §2 freezes the 6-slot pipeline (`nsl-resolve-params` → `nsl-expand-generate` → `nsl-expand-variables` → `nsl-explode-submod-array` → `nsl-inline-internal-func` (no-op slot per Q3 → Option B) → `nsl-check-semantics`)

### Implementing the LSP
- `design/nsl_tooling_design.md` lines **105–292** (full LSP spec: features, scheduler, hover example)
- `design/nsl_tooling_design.md` lines **48–104** (CST + incremental parse — LSP needs both)

### Adding a lint rule
- `design/nsl_tooling_design.md` lines **719–889** (linter architecture, rule interface, example)
- For a Sema-style rule, look at the matching constraint in `spec/nsl_lang.ebnf` 826–1015
- For a hardware rule (H001+), see lines **742–751** for the established list

### Working on the formatter
- `design/nsl_tooling_design.md` lines **578–718** (Wadler-Leijen pretty printer + NSL-specific rules)
- `design/nsl_tooling_design.md` lines **80–104** (CST layer the formatter walks)

### Working on the syntax highlighter
- `design/nsl_tooling_design.md` lines **293–577** (TextMate + tree-sitter grammars)
- `spec/nsl_lang.ebnf` lines **783–824** (reserved keywords — must match the highlighter's keyword list)

### Looking up an NSL keyword or grammar production
- `spec/nsl_lang.ebnf` §15 (lines **783–824**) — full keyword list
- For a specific production: see §4 below for the file's section TOC, then `view` that range only

### Looking up a semantic constraint S<n> or parser note N<n>
- All Sn: `spec/nsl_lang.ebnf` lines **826–1015** (read range `826 + 6*(n−1)` ish, or grep for `(S<n>)`)
- All Nn: `spec/nsl_lang.ebnf` lines **1017–1155**
- All Pn (preprocessor): `spec/nsl_pp.ebnf` lines **391–559**

### Driver / build / CLI flags
- `design/nsl_compiler_design.md` lines **1308–1366** (§11 Compilation class, `-emit=` flags)
- `design/nsl_compiler_design.md` lines **1405–1465** (§13 CMake layout, dependencies)
- M5 sibling contract `-emit=mlir`: `specs/008-m5-structural-passes/contracts/driver-emit-mlir.contract.md`
- M6 contract `-emit=hw`: `specs/010-m6-circt-lowering/contracts/driver-emit-hw.contract.md`

### Testing / CI
- `design/nsl_compiler_design.md` lines **1466–1479** (compiler testing strategy)
- `design/nsl_tooling_design.md` lines **968–973** (tooling test layout)

### Looking up the project milestone schedule

The milestone plan now lives in the project root, split across three files:

- Compiler track (`Mxx`–`Myy`), tooling track (`Txx`–`Tyy`), and compiler `P-*` (`P-CI`, `P-VEN`, `P-VCD`) → [`../README.md`](../README.md) §Roadmap
- "Which milestone delivers NSL feature X / `Sn` / `Nn` / `Pn`?" → [`../CLAUDE.md`](../CLAUDE.md) §1
- "Which milestone delivers LSP method / lint rule / formatter capability / highlighter scope / editor X?" → [`../CLAUDE.md`](../CLAUDE.md) §2
- Workflow `P-*` (`P-LIN`, `P-TS`) → [`../CONTRIBUTING.md`](../CONTRIBUTING.md) §3.8
- Rules for amending the milestone plan → [`../CONTRIBUTING.md`](../CONTRIBUTING.md) §3.9
- Constitutional anchors: every milestone cites the Constitution principle that gates its acceptance — see [`../.specify/memory/constitution.md`](../.specify/memory/constitution.md) Principles V, VI, VII, VIII, IX (and Governance "Milestone plan" clause).

### Looking up the project's AI development workflow

The 6-phase workflow (Linear → plan → implement → CodeRabbit self-review → PR → post-merge), the tool stack, the per-change-size application criteria, MCP servers, and pitfalls live in **[`/CONTRIBUTING.md`](../CONTRIBUTING.md) §3 Tooling and workflow** at the project root. Binding rules for the Linear / CodeRabbit / GitHub-Issues integrations live in `.specify/memory/constitution.md` "External Integrations" (around lines 421+) — `CONTRIBUTING.md` §3 is the operational playbook on top of that authority.

---

## 4. `spec/nsl_pp.ebnf` — section TOC (559 lines)

| Lines | Section |
|---|---|
| 1 | SPDX header |
| 2–64 | Header — role, notation, what's in the other file |
| 67–90 | §1 Top-level structure (line-oriented input model) |
| 92–102 | §2 Preprocessor directive — top-level alternation |
| 103–120 | §2.1 `#include` (quoted vs angle form) |
| 121–153 | §2.2 `#define` / `#undef` (incl. compile-time math in replacement) |
| 154–181 | §2.3 `#ifdef` / `#ifndef` / `#if` / `#else` / `#endif` |
| 182–234 | §2.4 `#line` directive (the one directive that survives the seam) |
| 236–310 | §3 Compile-time expression sub-grammar (`_int`, `_pow`, `_sin`, …) |
| 312–343 | §4 Macro reference syntax — `%IDENT%` splicing |
| 345–390 | §5 Lexical elements used by the preprocessor |
| 391–559 | Preprocessor notes **P1–P13** (semantics not expressible in EBNF) |
|  | • P1 line-orientation • P3 `%IDENT%` splicing • P5 helper return types • P8 `#include` search path • P12 boundary with NSL • P13 `#line` crosses the seam |

---

## 5. `spec/nsl_lang.ebnf` — section TOC (1155 lines)

| Lines | Section |
|---|---|
| 1 | SPDX header |
| 2–62 | Header — role, sources, notation |
| 65–78 | §1 Compilation unit |
| 81–108 | §2 Preprocessor seam (only `#line` survives into this grammar) |
| 110–118 | §3 Struct type declaration |
| 120–136 | §3.1 Top-level parameters |
| 138–178 | §4 `declare` block (ports, control terminals, modifiers) |
| 180–194 | §5 `module` block |
| 196–314 | §6 Internal structure elements (reg, wire, mem, proc_name, state_name, …) |
| 316–334 | §7 Function / Procedure / State definitions |
| 336–470 | §8 Action statements (par/alt/any/seq/if/for/while/generate) |
| 472–531 | §9 Atomic actions (transfers, control calls, finish, system tasks) |
| 533–593 | §10 System tasks (`_display`, `_finish`, `_init`, `_delay`, …) |
| 595–705 | §11 Expressions (incl. sign-extend `#`, zero-extend `'`, slice, concat) |
| 707–712 | §12 Width / constant expressions |
| 714–770 | §13 Lexical elements (identifiers, numbers, string literals, value Z/X/U) |
| 772–781 | §14 Whitespace and comments |
| 783–824 | §15 Reserved keywords (App. 3 corrected & augmented) |
| 826–1015 | **Semantic constraints S1–S29** (Sema's full responsibility) |
| 1017–1155 | **Parser notes N1–N14** (disambiguation, lexer hints) |

### Quick map of S/N constraints (so you can jump straight to what you need)

| Marker | About | File:line |
|---|---|---|
| S1 | `__` forbidden in identifiers | nsl_lang.ebnf:830 |
| S2 | wire has no init; only reg/struct-reg do | nsl_lang.ebnf:832 |
| S3 | `=` vs `:=` LHS rules | nsl_lang.ebnf:835 |
| S4 | func_in/out/self dummy-arg directions | nsl_lang.ebnf:838 |
| S5 | return-value terminal direction reversed | nsl_lang.ebnf:843 |
| S6 | proc_name args must be reg | nsl_lang.ebnf:848 |
| S7 | seq/while/for only inside func/proc body | nsl_lang.ebnf:850 |
| S8 | while/for only inside seq | nsl_lang.ebnf:854 |
| S9 | for-loop var must be reg | nsl_lang.ebnf:857 |
| S10 | generate loop var must be integer | nsl_lang.ebnf:860 |
| S11 | state_name proc-scope | nsl_lang.ebnf:863 |
| S12 | partial LHS only for variable | nsl_lang.ebnf:866 |
| S13 (constructive) | alt = priority, any = parallel | nsl_lang.ebnf:871 |
| S14 | conditional expr `else` mandatory | nsl_lang.ebnf:875 |
| S15 | bit-slice indices compile-time | nsl_lang.ebnf:878 |
| S16 | param_int/str only for HDL submodules | nsl_lang.ebnf:881 |
| S17 | system tasks need `simulation` modifier | nsl_lang.ebnf:885 |
| S18 (constructive) | struct MSB-first packing | nsl_lang.ebnf:890 |
| S19 (constructive) | one-clock per goto in seq | nsl_lang.ebnf:894 |
| S20 | `interface` modifier explicit clk/rst | nsl_lang.ebnf:899 |
| S21 | proc methods `.finish()` / `.invoke()` | nsl_lang.ebnf:903 |
| S22 | `return` only in func; width must match | nsl_lang.ebnf:934 |
| S23 (constructive) | reg width-omitted with init = 1-bit | nsl_lang.ebnf:939 |
| S24 (constructive) | mem partial init = zero-fill | nsl_lang.ebnf:944 |
| S25 | `goto` two kinds (label vs state) | nsl_lang.ebnf:949 |
| S26 | `func` ≡ `function` (canonical: `func`) | nsl_lang.ebnf:964 |
| S27 (constructive) | control-terminal name as 1-bit value | nsl_lang.ebnf:970 |
| S28 | `first_state` positioning rules | nsl_lang.ebnf:992 |
| S29 | `_init` block placement | nsl_lang.ebnf:1007 |
| N1 | `if` statement-vs-expression | nsl_lang.ebnf:1020 |
| N2 | `&` `\|` `^` reduction-vs-bitwise | nsl_lang.ebnf:1029 |
| N3 | `.{` two-character lookahead | nsl_lang.ebnf:1033 |
| N5 | `#` line-marker vs sign-extend | nsl_lang.ebnf:1041 |
| N6 | proc-instance method access | nsl_lang.ebnf:1057 |
| N7 | dotted `func` def for submodule out | nsl_lang.ebnf:1067 |
| N10 | `label` reserved (mostly unused) | nsl_lang.ebnf:1081 |
| N11 | three classes of `_`-prefix names | nsl_lang.ebnf:1089 |
| N14 | `#line` source-location tracking | nsl_lang.ebnf:1119 |

---

## 6. `design/nsl_compiler_design.md` — section TOC (1509 lines)

Line ranges below verified 2026-05-04 against the file post-M4
amendments #9 + #10 + the M5/M6 commentary additions.

| Lines | Section |
|---|---|
| 1–2 | SPDX header + blank |
| 3–17 | §1 Design Goals and Constraints |
| 20–128 | §2 Overall Pipeline (ASCII flowchart end-to-end) |
| 132–148 | §3 Layered Architecture (9-library breakdown table) |
| 152–295 | §4 Class Diagram Overview (Mermaid) |
| 299–615 | §5 AST Class Hierarchy (3 Mermaid diagrams: Decl/Stmt/Expr) |
| 617–685 | §5.x AST node skeleton (C++17 code) |
| 688–814 | §6 Symbol Table — Symbol class hierarchy + scopes table (incl. constructive-`Sn` carve-out note) |
| 815–877 | §6.x Type System — TypeSystem code |
| 878–1135 | §7 The `nsl` MLIR Dialect (op summary incl. M4 marker / lowering-helper consolidation, rationale, TableGen) |
| 1136–1217 | §8 Lowering: AST → `nsl` dialect (visitor + per-node rule table) |
| 1218–1234 | §9 Structural Expansion Passes (NSL-dialect local) |
| 1235–1305 | §10 Lowering: `nsl` → CIRCT (per-op mapping table; M6 contract `circt-lowering.contract.md` §1 freezes this table by reference) |
| 1308–1366 | §11 Driver / Compilation Object (CompileOptions, run loop) |
| 1367–1404 | §12 Error Handling and Diagnostics (DiagnosticEngine, FixItHint) |
| 1405–1465 | §13 Build System and Dependencies (CMake, repo layout) |
| 1466–1479 | §14 Testing Strategy (lexer→e2e+formal layers) |
| 1480–1500 | §14.5 Milestone Plan (routing pointer to `../../README.md` §Roadmap, `../../CLAUDE.md` §1, and the Constitution; do not duplicate the table) |
| 1501–1509 | §15 Extension Points (verif, LSP, alternate backends) |

---

## 7. `design/nsl_tooling_design.md` — section TOC (1015 lines)

| Lines | Section |
|---|---|
| 1–2 | SPDX header + blank |
| 3–45 | §1 Design Philosophy — One Front-End, Five Frontdoors |
| 48–102 | §2 Shared Infrastructure (incremental parse, stable IDs, token lattice, CST) |
| 105–148 | §3.1 LSP Overall Architecture (clangd-style 3 layers) |
| 150–168 | §3.2 LSP Features by Difficulty (table) |
| 170–201 | §3.3 Incremental Document Management (TUScheduler) |
| 203–259 | §3.4 LSP Class Diagram |
| 261–289 | §3.5 Hover flow example (C++ code) |
| 293–303 | §4 Syntax Highlighter — two-tier strategy |
| 304–358 | §4.1 Token Categories (TextMate scope names) |
| 360–462 | §4.2 TextMate Grammar Skeleton (JSON) |
| 464–561 | §4.3 Tree-sitter Grammar Skeleton (JS + queries) |
| 563–575 | §4.4 Editor Integration Matrix |
| 578–597 | §5.1 Formatter design (opinionated, configurable) |
| 598–664 | §5.2 Formatter Architecture (Wadler-Leijen + Doc IR) |
| 666–701 | §5.3 NSL-Specific Formatting Rules |
| 703–716 | §5.4 Diff-style CLI |
| 719–752 | §6.1 Linter — Three Rule Tiers (W/S/H rules) |
| 753–790 | §6.2 Linter Architecture |
| 792–835 | §6.3 Rule Interface (C++) |
| 836–859 | §6.4 Example Rule (UnusedRegRule) |
| 861–879 | §6.5 Lint Configuration (TOML) |
| 881–889 | §6.6 CI Integration |
| 891–909 | §7 Cross-Tool Integration — LSP as hub |
| 913–973 | §8 Shared Directory Layout |
| 977–997 | §9 Milestone Plan (routing pointer to `../../README.md` §Roadmap, `../../CLAUDE.md` §2, and `../../CONTRIBUTING.md` §3.8–§3.9) |
| 998–1015 | §10 Summary — Value Proposition |

---

## 8. Cross-references between spec and design

When you touch one of these areas, both sides are involved:

| Topic | spec | design |
|---|---|---|
| Lexical reserved words | nsl_lang.ebnf:783–824 | nsl_compiler_design.md (Lexer, §3 layer 3); nsl_tooling_design.md:304–358 (highlighter) |
| `_`-prefix system names | nsl_lang.ebnf:1083–1105 (N11) | nsl_compiler_design.md (Lexer, §3 layer 3) |
| `#line` directive | nsl_pp.ebnf:516–559 (P13); nsl_lang.ebnf:1119–1155 (N14) | nsl_compiler_design.md (SourceLocation in §6 Symbol Table area, §12 Diagnostics) |
| Compile-time helpers | nsl_pp.ebnf:236–310, P5/P7 | nsl_compiler_design.md (Preprocessor in §3, expansion in §9) |
| `%IDENT%` macros | nsl_pp.ebnf:312–343, P3 | nsl_compiler_design.md §9 (`NSLCheckSemanticsPass` checks residue-free) |
| AST shape | nsl_lang.ebnf §§1–11 | nsl_compiler_design.md:299–685 |
| Sema constraints S1–S29 | nsl_lang.ebnf:826–1015 | nsl_compiler_design.md:688–877 (SymbolTable/TypeSystem); nsl_tooling_design.md:723–741 (lint W/S elevations) |
| Parser disambiguation N1–N14 | nsl_lang.ebnf:1017–1155 | nsl_compiler_design.md (Parser, §3 layer 4) |
| `proc`/`state`/`finish` semantics | nsl_lang.ebnf:1051–1059 (N6); 900–929 (S21) | nsl_compiler_design.md:878–1135 (dialect ops, §7); 1235–1305 (FSM lowering, §10) |
| `seq` / `while` / `for` placement | nsl_lang.ebnf:850–858 (S7–S9) | nsl_compiler_design.md:1235–1305 (`nsl.seq` → fsm.machine, §10) |
| `generate` unrolling | nsl_lang.ebnf §8 + S10 | nsl_compiler_design.md:1218–1234 (`NSLExpandGeneratePass`, §9) |
| Hover / definition / refs | — | nsl_tooling_design.md:105–289 |
| Lint rule W/S/H taxonomy | overlaps S1–S29 | nsl_tooling_design.md:719–752 |

---

## 9. Editing protocol

The full contributor-facing protocol — including AI-assistant attribution rules — is in [`../CONTRIBUTING.md`](../CONTRIBUTING.md). The essentials, restated for quick reference:

1. **Spec changes are load-bearing.** Edit `spec/*.ebnf` only when adding/removing a real language feature. Note the change in the file's header comment with date.
2. **When you change the spec, update `design/nsl_compiler_design.md`** wherever the cross-reference table in §8 lists a corresponding section. The compiler test suite (§14, lines 1452–1464) should grow a regression for the new behaviour.
3. **`design/` changes alone** are common (a new pass, a refactored class). They don't trigger spec changes — but check §8 to confirm you're not contradicting the spec.
4. **Adding a Sn / Nn / Pn note**: keep numbering monotonic; never reuse a retired number — add e.g. `(S30)` and update §5 of this file.
5. **PDF reference manuals** (`NSL_Language_Reference_ver1.4E.pdf`, `NSLTUT20151006_E.pdf`, `NSL_Tutorial20160121.pdf`) are external sources cited in `spec/*.ebnf` headers. They are **not** in this directory by design — `docs/` distills them. If the PDFs and `docs/` disagree, this directory's interpretation (with audited open-source NSL projects as ground truth) wins.

If you are an AI assistant making a commit on behalf of a user: read `../CONTRIBUTING.md` §§4–5 carefully. You must not add `Signed-off-by`, and you must add an `Assisted-by:` trailer.

---

## 10. Upstream sources (NSL origin)

NSL is a hardware description language by Overtone Corporation. The
specification in `docs/spec/` is **distilled here** from the 2012
Reference Manual, the 2015 English tutorial, the 2016 Japanese
tutorial, and roughly 12,000 lines of audited open-source NSL projects
(`rv32x_dev`, `turboV`, `mmcspi`, `SDRAM_Controler`,
`mips32_single_cycle`, `ahb_lite_nsl`, `cpu16`).

The upstream PDFs are **not** committed to this repository — `docs/`
distills them. If you want the originals:

- <https://www.overtone.co.jp/Document/NSL_Language_Reference_ver1.4E.pdf>
- <https://www.overtone.co.jp/Document/NSLTUT20151006_E.pdf>
- <https://www.overtone.co.jp/Document/NSL_Tutorial20160121.pdf>

If `docs/spec/` and a PDF disagree, this directory's interpretation
(with the audited open-source NSL projects as the tiebreaker) wins by
policy. The header comment of the relevant `.ebnf` file should record
the rationale; if it does not, that is itself a bug worth reporting.

---

## 11. Documentation contribution checklist

In addition to the project-wide PR checklist in
[`../CONTRIBUTING.md`](../CONTRIBUTING.md) §7, when your PR touches
`docs/` confirm:

- [ ] If `docs/spec/` changed, the corresponding `docs/design/`
  sections were updated (cross-references in §8 above).
- [ ] If a new `Sn` / `Nn` / `Pn` was added, the quick-map in §5 was
  updated.
- [ ] Line ranges in §§4–7 are still accurate for any file whose
  section boundaries shifted.
- [ ] No upstream NSL PDF files were committed (see §10).

If you find a contradiction between `docs/spec/` and `docs/design/`,
treat it as a bug in `docs/design/` and report it — `docs/spec/` wins
by policy. Open an issue describing both sides and citing the
specific lines in each file.

---

## 12. Convenience commands

```bash
# Find a semantic constraint by number:
grep -n '(S17)' spec/nsl_lang.ebnf

# Find a parser note by number:
grep -n '(N5)'  spec/nsl_lang.ebnf

# Find a preprocessor note by number:
grep -n '(P3)'  spec/nsl_pp.ebnf

# Find a compiler-design section:
grep -n '^## '  design/nsl_compiler_design.md

# Find a tooling-design section:
grep -n '^## \|^### ' design/nsl_tooling_design.md

# Find every place a keyword is mentioned:
grep -rn '\bproc_name\b' spec/ design/
```
