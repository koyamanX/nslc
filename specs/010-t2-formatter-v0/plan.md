<!-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception -->

# Implementation Plan: T2 — Formatter v0 (`nsl-fmt`)

**Branch**: `010-t2-formatter-v0` | **Date**: 2026-05-04 | **Spec**: [spec.md](./spec.md)
**Input**: Feature specification from `/specs/010-t2-formatter-v0/spec.md`

## Summary

T2 delivers the first NSL code formatter — a `gofmt`/`rustfmt`/
`black`-style canonical pretty-printer (`nsl-fmt`) plus a reusable
library (`libNslFmt.a`) that T5 will later wire into the LSP's
`textDocument/formatting` and `textDocument/rangeFormatting`
handlers (T5 itself is out of scope at T2). The user-visible
deliverable is the `nsl-fmt` binary with the seven CLI flags
specified in [`docs/design/nsl_tooling_design.md`](../../docs/design/nsl_tooling_design.md)
§5.4 (`-i`, `-c`/`--check`, `--stdin`, `--config`, `--range`, plus
positional file args), the six NSL-specific layout rules in §5.3,
and the ten configuration knobs in §5.1 — all covered by a
lit + FileCheck corpus that gates audited-project-corpus
idempotence in CI.

The technical approach (see [research.md](./research.md) for
Decision/Rationale/Alternatives per choice): a thin
**directive-aware pre-pass** (research §1, locking
Q1 → Option A from `/speckit-clarify`) splits raw NSL source into
`(DirectiveTok | NSLFragment)+`; each NSL fragment is parsed by the
existing `libNSLFrontend.a` parser extended with a **CST-emitting
mode** (research §2 — preserves trivia + reuses every existing
production); a Wadler–Leijen `Doc`-IR layout planner (research §3)
emits typed layout commands; a ribbon-fitting renderer produces the
final byte stream. Configuration is parsed by the **`toml++`**
single-header library vendored under `third_party/` (research §4).
The CLI uses LLVM's `cl::opt` (already in the project's link
graph). `--check` diffs use a small in-tree unified-diff
generator (research §5). The `nsl-fmt` binary and `libNslFmt.a`
share a single `format_buffer()` entry point so the CLI-vs-library
parity test in FR-022 reduces to a one-line GoogleTest
(quickstart §3).

## Technical Context

**Language/Version**: C++17 across `nsl-fmt` and `libNslFmt.a`
(Constitution "Build, Code, and Licensing Standards"). C++20
features (`std::format`, ranges, concepts) prohibited.

**Primary Dependencies**:
- `libNSLFrontend.a` (preprocessor, lexer, parser, AST, sema,
  symbol table, diagnostics) — Principle II's user-facing-tooling
  reuse rule; T2 extends `nsl-parse` with a CST-emitting mode
  but does not duplicate any production.
- LLVM 18 `Support` (`StringRef`, `Twine`, `raw_ostream`,
  `FileSystem`, `MemoryBuffer`, `cl::opt`) — already linked by
  every existing layer.
- `toml++` v3.4 single-header library (vendored under
  `third_party/tomlpp/`, `MIT` license — research §4 for the
  alternatives evaluated).

**Storage**: N/A. `nsl-fmt` is a one-shot stateless CLI; there is
no persistent state.

**Testing**:
- **lit + FileCheck** for the bulk of the `test/Fmt/` corpus
  (FR-020, FR-021): one fixture per §5.3 NSL-specific rule, one
  per CLI surface element, one per directive class (Q1
  pass-through), one per edge case, one per audited-corpus file
  (idempotence gate). Constitution Principle VI's per-layer
  accepted-driver clause permits any conventional driver for
  tool-level tests; the project has standardized on lit, so we
  follow that.
- **GoogleTest** (`test_unit/Fmt/`) for FR-022 (CLI ↔ library
  parity), the directive-aware pre-pass invariants (CST shape +
  Z/X/U literal preservation), the unified-diff emitter, and the
  TOML config-record parsing edge cases.
- **Audited corpus** (`test/audited/<project>/`, vendored per
  Principle V `P-VEN` — see research §6 for current status) is
  the ground-truth input set for SC-002.

**Target Platform**: Linux x86_64 (Constitution Principle IX
build matrix). Dev container is canonical
(`ghcr.io/koyamanx/nsl-nslc:dev`).

**Project Type**: Tooling library + CLI binary (single project,
LLVM-style layered architecture per Constitution Principle II).
`nsl-fmt` is one of the three named user-facing tooling binaries
in Principle II (alongside `nsl-lsp` and `nsl-lint`); T2 is the
first to land.

**Performance Goals**:
- SC-003: `nsl-fmt --check` on a 1000-line NSL file completes in
  under 250 ms wall-clock on the dev-container hardware target.
- T2 lit corpus completes in under 30 s in CI (matches Principle
  IX stage 4 timing budget for lowering tests).

**Constraints**:
- **Determinism (Principle V)**: `nsl-fmt` output is purely a
  function of (source bytes, configuration record, CLI flag list).
  No environment dependence (locale, hostname, build path,
  source-file mtime, CWD). The CI grep introduced by M5
  (`scripts/audit_determinism.sh`) extends to `lib/Fmt/`: forbids
  `std::unordered_*`, pointer-derived ordering, time sources.
- **No-duplication (Principle II)**: the directive-aware pre-pass
  is the **only** net-new parsing code T2 owns; NSL fragments go
  through the existing `libNSLFrontend.a` parser. CI grep enforces
  no second-implementation of NSL grammar productions outside
  `lib/Parse/`.
- **Source-locating diagnostics (Principle IV)**: every diagnostic
  emitted by `libNslFmt.a` (config error, parse error, range out
  of bounds) routes through the existing `basic::DiagnosticEngine`
  with a precise `file:line:col`.
- **Idempotence (FR-008)**: `format_buffer(format_buffer(s)) ==
  format_buffer(s)` for every accepted `s`. Enforced by
  `test/Fmt/idempotence/` lit-fixture sweep on every audited file.
- **Single public umbrella header (Principle II)**: `Fmt.h` is
  the only public header; `nsl-fmt` is NOT one of the named
  exceptions for `nsl-ast` / `nsl-sema`.

**Scale/Scope**:
- ~10 public symbols exported from `Fmt.h` (count frozen by
  [`format-api.contract.md`](./contracts/format-api.contract.md)
  §5).
- ~30 `visit()` overrides on `LayoutPlanner` (one per NSL CST
  node category; mirrors the AST visitor count from M2/M5).
- ~150 LOC for the directive-aware pre-pass (raw-input scanner +
  fragment iterator).
- ~300 LOC for the Wadler–Leijen `Doc`/renderer pair (the
  algebra is small; the layout decisions live in the planner).
- ~400 LOC `LayoutPlanner` (the bulk of project-specific work).
- ~50 LOC `Config` parser (toml++ does the heavy lifting).
- ~50 LOC unified-diff emitter.
- **Test corpus**: ~25 lit fixtures across rules/CLI/directives/
  edge + 7 audited-corpus idempotence fixtures + ~5 GoogleTest
  cases = ~37 distinct test files at T2 acceptance.

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-check after Phase 1
design.*

### Phase 0 (pre-research) gate

| Principle | Status | Notes |
|---|---|---|
| I. Spec Is Authoritative | **Pass** | T2 introduces no new `Sn`/`Nn`/`Pn`. The directive-aware pre-pass *consumes* existing `pp.ebnf` directives (P3, P8, P12, P13 etc.) without redefining their grammar. The "no silent AST drops" sub-clause does not apply at the formatter layer (the formatter does not produce an AST consumed by later compiler stages). |
| II. Layered Library Architecture | **Pass** | `nsl-fmt` is one of the three Principle-II-named user-facing tooling binaries; `libNslFmt.a` is its tool-private library (parallel to future `libNslLsp.a` / `libNslLint.a`). Reuses `libNSLFrontend.a` for lexer / parser / diagnostics per the no-duplication rule (FR-018). Single public umbrella header `Fmt.h` (NOT one of the named exceptions). The directive-aware pre-pass is registered as a CST-mode extension of `nsl-parse` rather than a parallel parser. |
| III. Stock CIRCT Below | **Pass (vacuous)** | `nsl-fmt` is far above the dialect/CIRCT seam; zero `circt::*` / `mlir::*` references in `lib/Fmt/`. |
| IV. Source-Locating Diagnostics | **Pass** | FR-016 / FR-018 route every formatter diagnostic (config error, parse error, range out of bounds) through `basic::DiagnosticEngine`. The directive pre-pass and CST mode preserve `SourceRange` on every node so error reporting carries `file:line:col`. |
| V. Inspectable, Deterministic Pipeline | **Pass (with carve-out note)** | The `-emit=` clause of Principle V applies to *compilation pipeline* stages; `nsl-fmt` is not a compilation stage and so does not need an `nslc -emit=fmt` flag. The byte-stability clause applies in full and is satisfied by FR-008 (idempotence) + FR-011 (literal preservation) + the determinism CI grep extending to `lib/Fmt/`. Carve-out is consistent with how `nsl-opt` (also a tool, not a pipeline stage) is handled. |
| VI. Layered Test Discipline | **Pass** | Tool-level tests use lit + FileCheck per the per-layer accepted-driver clause ("Lexer/parser/sema/dialect tests MAY use any conventional driver" — formatter testing follows the same flexibility). FR-020 / FR-021 / FR-022 enumerate the corpus shape; SC-002 wires the audited-corpus idempotence sweep into CI. |
| VII. Spec ↔ Design Coupling | **Pass** | T2 touches no `docs/spec/*.ebnf` (no new `Sn`/`Nn`/`Pn`). T2 amends `docs/design/nsl_tooling_design.md` §5.2 to add the directive-aware pre-pass to the architecture diagram (research §2). T2 amends `docs/CLAUDE.md` §7 line ranges if §5 boundaries shift. T2 amends project-root `CLAUDE.md` §2.3 footnote to clarify the directive-aware-pre-pass scope decision. All three updates land in the same change as the implementation. |
| VIII. Test-First Development | **Pass** | Quickstart §3 prescribes the first failing fixture (an `alt`-block alignment test that fails with `nsl-fmt`'s M0 stub). Subsequent T-tasks follow Red → Green → Refactor at every layer. The new-`Sn` clause does not apply (no new `Sn`); the bug-fix clause does not apply (greenfield); the refactor clause does not apply (greenfield). |
| IX. Continuous Integration & Delivery | **Pass** | T2 deliverables exercise: stage 1 (build matrix — adds `tools/nsl-fmt/` and `lib/Fmt/` to the existing build); stage 2 (static checks — `clang-format`, `clang-tidy`, SPDX header on every new file); stage 3 (unit & layer — adds `check-nsl-fmt` ninja target); stage 4 (lowering tests — adds `test/Fmt/` to the lit registration). Stages 5/6 are vacuous for T2 (formatter does not produce Verilog, does not engage `riscv-formal`). New CI step `nsl-fmt --check test/audited/**/*.nsl` enforces SC-002. |

**Gate result**: ✅ All nine principles pass at the pre-research
gate. The Principle V carve-out (no `-emit=fmt` for a non-pipeline
tool) is documented inline rather than under Complexity Tracking
because it does not represent a violation — the principle's
`-emit=` clause is explicitly scoped to compilation stages.

### Phase 1 (post-design) re-check

After authoring research.md / data-model.md / contracts/ /
quickstart.md, the gate is re-evaluated.

| Principle | Re-evaluation result |
|---|---|
| II. Layered Library Architecture | **Confirmed** — `format-api.contract.md` §5 freezes the 10-symbol public surface of `Fmt.h`; `data-model.md` §1 confirms internal-header layout under `lib/Fmt/` is private. `lib/Fmt/CMakeLists.txt` declares only the two permitted `LINK_LIBS` (`nsl-frontend`, `tomlpp`). |
| IV. Source-Locating Diagnostics | **Confirmed** — `data-model.md` §3 invariant + `format-api.contract.md` §4 (return-`Result` shape carries `SourceRange` for every diagnostic); `cst-shape.contract.md` §3 forbids any CST node without a `SourceRange`. |
| V. Inspectable, Deterministic Pipeline | **Confirmed** — `format-api.contract.md` §6 freezes determinism axes (input bytes + Config record + CLI flags only); CI grep enforces no `std::unordered_*` in `lib/Fmt/`. |
| VI. Layered Test Discipline | **Confirmed** — `data-model.md` §7 catalogs the four fixture-corpus shapes; FR-020 / FR-021 / FR-022 each have a §7 sub-section. |
| VII. Spec ↔ Design Coupling | **Confirmed** — quickstart §9 lists the three documentation updates (project `CLAUDE.md` §2.3 footnote; `docs/design/nsl_tooling_design.md` §5.2 architecture diagram; `docs/CLAUDE.md` §7 line ranges). |
| VIII. Test-First Development | **Confirmed** — `formatting-rules.contract.md` §5 freezes the diagnostic-string per fixture (parse-error refusal, mutually-exclusive-flag rejection, range-out-of-bounds); quickstart §3 prescribes the failing-first commit. |
| IX. Continuous Integration & Delivery | **Confirmed** — quickstart §10 enumerates the per-stage CI gates; the new `nsl-fmt --check audited/**/*.nsl` step lands in `scripts/ci.sh` (research §6 for audited-corpus availability). |

**Phase 1 gate result**: ✅ No new violations introduced by
design. Complexity Tracking section below is empty.

## Project Structure

### Documentation (this feature)

```text
specs/010-t2-formatter-v0/
├── plan.md                                  # This file
├── research.md                              # Phase 0 — Decision/Rationale/Alternatives × 9
├── data-model.md                            # Phase 1 — entity catalog
├── quickstart.md                            # Phase 1 — developer onboarding
├── contracts/
│   ├── cli-surface.contract.md              # CLI flag + exit-code freeze
│   ├── format-api.contract.md               # 10-symbol library surface freeze
│   ├── cst-shape.contract.md                # CST + DirectiveTok + Trivia freeze
│   └── formatting-rules.contract.md         # Six NSL-specific rules as I/O assertions
├── checklists/
│   └── requirements.md                      # Spec quality checklist (from /speckit-specify)
├── spec.md                                  # The feature specification
└── tasks.md                                 # Phase 2 — /speckit-tasks output (NOT created here)
```

### Source Code (repository root)

The T2 deliverable touches the following trees:

```text
include/nsl/Fmt/
└── Fmt.h                                    # NEW: public umbrella (10 symbols frozen)

lib/Fmt/
├── CST.h                                    # NEW: CSTNode + Trivia + DirectiveTok types
├── CST.cpp
├── DirectiveSplitter.h                      # NEW: raw → directive | NSL-frag pre-pass (FR-012a)
├── DirectiveSplitter.cpp
├── CSTBuilder.h                             # NEW: NSL fragment + Trivia → CST visitor (extends nsl-parse)
├── CSTBuilder.cpp
├── Doc.h                                    # NEW: Wadler–Leijen Doc IR (Text/Line/Nest/Group/Concat/Align/Comment)
├── Doc.cpp
├── LayoutPlanner.h                          # NEW: CST → Doc visitor (~30 visit() overrides)
├── LayoutPlanner.cpp
├── LayoutRenderer.h                         # NEW: Doc → string (ribbon-fitter, max_line_length)
├── LayoutRenderer.cpp
├── Config.h                                 # NEW: TOML parser + Configuration record (10 keys)
├── Config.cpp
├── ConfigDiscovery.h                        # NEW: upward .nsl-fmt.toml walk (FR-013)
├── ConfigDiscovery.cpp
├── Format.h                                 # NEW: top-level format_buffer() entry (FR-017)
├── Format.cpp
├── Diff.h                                   # NEW: unified-diff emitter (--check)
├── Diff.cpp
└── CMakeLists.txt                           # NEW: add_nsl_library(nsl-fmt LINK_LIBS nsl-frontend tomlpp)

tools/nsl-fmt/
├── main.cpp                                 # NEW: CLI driver (~150 LOC; uses cl::opt)
└── CMakeLists.txt                           # NEW: add_nsl_executable(nsl-fmt LINK_LIBS nsl-fmt)

third_party/tomlpp/
├── toml.hpp                                 # NEW: vendored toml++ v3.4 (single header)
├── LICENSE                                  # NEW: MIT
└── PROVENANCE.md                            # NEW: upstream URL + commit SHA + license

include/nsl/Parse/Parser.h                   # AMEND: add `class CSTSink` virtual interface + `Parser::setEmitCST(CSTSink*)` method (the only new symbols on nsl-parse's existing single public header — Principle II keeps the single-header rule for nsl-parse)
lib/Parse/
└── CSTMode.cpp                              # NEW: private impl of Parser::setEmitCST + the gated emit-event call sites (no public header)

test/Fmt/
├── rules/                                   # FR-009 / FR-020 — six §5.3 rules
│   ├── alt-case-alignment/
│   ├── struct-member-alignment/
│   ├── proc-name-arg-wrap/
│   ├── bit-slice-spacing/
│   ├── operator-spacing/
│   └── attached-comments/
├── cli/                                     # FR-001..FR-007 + FR-003a CLI surface
│   ├── stdin/
│   ├── in-place/
│   ├── check-mode/
│   ├── range/
│   ├── multi-file/                          # FR-003a continue-on-error
│   └── mutually-exclusive/                  # FR-006
├── config/                                  # FR-013..FR-016
│   ├── discovery/
│   ├── unknown-key/
│   ├── invalid-value/
│   └── explicit-config/
├── directives/                              # Q1 — preprocessor pass-through
│   ├── include-passthrough/
│   ├── ifdef-island/
│   ├── define-passthrough/
│   ├── line-passthrough/
│   └── ident-splice-passthrough/
├── edge/                                    # Edge Cases section of spec
│   ├── empty-input/
│   ├── crlf-normalization/
│   ├── bom-preserve/
│   ├── parse-error-refusal/                 # FR-012
│   └── over-long-line/
├── idempotence/                             # FR-021 + SC-002
│   ├── audited/                             # one fixture per audited project
│   │   ├── rv32x_dev.test
│   │   ├── turboV.test
│   │   ├── mmcspi.test
│   │   ├── SDRAM_Controler.test
│   │   ├── mips32_single_cycle.test
│   │   ├── ahb_lite_nsl.test
│   │   └── cpu16.test
│   └── synthetic/                           # constructed-edge idempotence cases
└── lit.cfg.py                               # AMEND project-root lit.cfg.py to register test/Fmt/

test_unit/Fmt/
├── format_parity_test.cc                    # FR-022 — CLI ↔ library parity
├── directive_splitter_test.cc               # CST shape + literal preservation
├── config_parser_test.cc                    # TOML edge cases
├── doc_layout_test.cc                       # Wadler–Leijen primitives
└── diff_emitter_test.cc                     # unified-diff correctness

scripts/
├── ci.sh                                    # AMEND: add `nsl-fmt --check test/audited/**/*.nsl` step
└── audit_fmt_determinism.sh                 # NEW (or extend existing audit_determinism.sh): forbid std::unordered_* / pointer-iter / time sources in lib/Fmt/

CLAUDE.md                                    # AMEND: SPECKIT marker block points to specs/010-t2-formatter-v0/plan.md
docs/design/nsl_tooling_design.md            # AMEND: §5.2 architecture diagram includes the directive-aware pre-pass (research §2)
docs/CLAUDE.md                               # AMEND: §7 line ranges if §5 boundaries shift
README.md                                    # UNCHANGED at T2 (T-track table already lists T2)
```

**Structure Decision**: Single project (Option 1 from the plan
template), LLVM-style, layered library architecture per
Constitution Principle II. T2 introduces `nsl-fmt` as the first of
the three named user-facing tooling binaries. No `frontend/` /
`backend/` split — the formatter is a library + a thin CLI on top.

The two-line amendment to `include/nsl/Parse/Parser.h` (a new
abstract `CSTSink` interface + a `Parser::setEmitCST(CSTSink*)`
method) plus the private impl file `lib/Parse/CSTMode.cpp` are
the only modifications to `libNSLFrontend.a` itself. Together
they add ONE new public symbol (`CSTSink`) to `nsl-parse`'s
existing single public header — Principle II's single-header
rule for `nsl-parse` is preserved (no second header). The
`CSTSink` is implemented above the layer boundary (by
`nsl-fmt`'s `CSTBuilder`), keeping dependency direction
downward. This is Principle II's preferred shape: extend the
shared layer through a layered observer interface, don't
duplicate parsing code and don't widen the public surface.

## Complexity Tracking

> **Fill ONLY if Constitution Check has violations that must be
> justified**

Empty — no Constitution violations surfaced at either gate. The
Principle V `-emit=` carve-out is documented inline at the
pre-research gate; it is not a violation, and the same precedent
applies to the existing `nsl-opt` tool (also not a compilation
pipeline stage).

## Phase Cross-References

- **Phase 0 (research)**: [`research.md`](./research.md) §§1–9 —
  nine Decision/Rationale/Alternatives entries covering Q1 (the
  directive-aware pre-pass), Q2 (`--range` shape), Q3 (multi-file
  continue-on-error), the TOML library choice, the Wadler–Leijen
  reference impl, the audited-corpus availability, the CST-mode
  parser-extension shape, the unified-diff implementation choice,
  and the `cl::opt` vs custom argv parser decision.
- **Phase 1 (data model)**: [`data-model.md`](./data-model.md) —
  seven entity sections (CSTNode/Trivia/DirectiveTok, Doc IR,
  LayoutPlanner, LayoutRenderer, Configuration, Format result,
  Diff record).
- **Phase 1 (contracts)**: [`contracts/`](./contracts/) — four
  `.contract.md` files freezing the public surfaces (CLI flag /
  exit-code matrix, 10-symbol library API, CST taxonomy,
  six-rule input/output assertions).
- **Phase 1 (quickstart)**: [`quickstart.md`](./quickstart.md) —
  developer onboarding with the suggested T-1..T-N implementation
  order, build/test loop, idempotence-verification recipe,
  pre-merge checklist, and the three documentation updates that
  land in the same PR.
- **Phase 2 (tasks)**: NOT generated by this command — run
  `/speckit-tasks` next.
