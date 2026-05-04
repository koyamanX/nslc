<!-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception -->

# Research — T1 TextMate Grammar + Language Configuration

**Feature**: `009-t1-textmate-grammar`
**Phase**: 0 (research / unknowns resolution)
**Date**: 2026-05-04

This document resolves every plan-level open question raised during
spec authoring and Constitution-Check evaluation. Each section
records: **Decision** / **Rationale** / **Alternatives considered**.

---

## 1. Scope-test runner

**Decision**: **`vscode-tmgrammar-test`** — the npm package by
`@PanAeon/vscode-tmgrammar-test`.

**Rationale**:
- De-facto standard. Microsoft's own first-party VS Code grammar
  packages (e.g. `vscode/extensions/typescript-basics/test/`) use
  the same assertion DSL — `// >...^` lines that point at the
  fixture line above and assert the expected scope.
- Self-contained Node.js binary — runs in the existing dev
  container's Node toolchain (the dev container ships Node for the
  CodeRabbit / `lit` reporter integrations) without adding a new
  language runtime.
- Output is human-readable diff + machine-readable exit code (0 =
  green, non-zero = at-least-one-failed). Compatible with the
  `./scripts/ci.sh` exit-code dispatch.
- Fixture authoring is colocated: the assertions live next to the
  fixture, not in a separate JSON, so adding a new keyword reads
  more like adding a unit test than configuring a runner.

**Alternatives considered**:
- **Hand-rolled Python runner using `vscode-textmate` Node bindings
  via subprocess** — rejected: reinvents what `vscode-tmgrammar-test`
  already does, and adding the bindings still requires Node (no
  cost saved). Loss of the de-facto-standard assertion DSL is a
  net-negative for first-time tooling-track contributors.
- **Snapshot-test approach** (run the grammar against a fixture, dump
  the entire scope tree to a `.golden`, diff) — rejected: excessive
  diff churn on minor scope-name refinements; loses the
  per-assertion failure granularity that point-in-fixture assertions
  give. Snapshot tests are the right shape for AST snapshots
  (precedent: `test/parse/grammar/`) but wrong for token-level
  scope coverage.
- **Skip the runner; ship only the fixture for manual review** —
  rejected: violates `README.md §Roadmap` row T1's stated test gate
  ("TextMate scope tests on a fixture file …") and Constitution
  Principle VIII (TDD).

**Containerization implication**: the dev container
`ghcr.io/koyamanx/nsl-nslc:dev` already ships Node (per the user's
auto-memory entry on the dev container — Node was added for the
CodeRabbit hook). If a future image rebuild drops Node, the T1 CI
cell breaks; the rebuild PR would surface this and add Node back.

---

## 2. SPDX header convention for JSON artefacts

**Decision**: Use the **`_comment_top` JSON-key** convention,
precedent: `.github/branch-protection.json`.

**Rationale**:
- JSON has no comment syntax. The Constitution requires SPDX
  headers in "the comment syntax appropriate to the file format"
  — for JSON, an in-band JSON-key carrying the SPDX string is the
  only first-class option that survives JSON parsing.
- The project already does this. `.github/branch-protection.json`
  starts:
  ```json
  {
    "_comment_top": "SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception. JSON has no comment syntax; …",
    …
  }
  ```
  Following this precedent keeps SPDX-checker logic uniform.
- TextMate runtimes ignore unknown top-level keys (the grammar
  schema does not require additionalProperties: false), so
  `_comment_top` does not affect grammar behaviour.
- Same applies to `language-configuration.json` — VS Code ignores
  unknown top-level keys.

**Alternatives considered**:
- **JSONC** (JSON with comments) — rejected: TextMate runtime in
  several editors (notably the GitHub Linguist path) only accepts
  strict JSON; emitting a JSONC variant would gate on which
  consumer parses it.
- **Sidecar `.LICENSE` file** — rejected: the SPDX-checker
  (`scripts/check_spdx.py`) reads file-1's content and would either
  need a special case for JSON or treat all JSON as exempt. Both
  alternatives lose SPDX coverage on the most user-visible
  artefacts.
- **Add to `scripts/spdx_exceptions.txt`** — rejected: the
  `_comment_top` solution gives genuine SPDX presence; falling back
  to an exception silently loses license tracking.

---

## 3. Built-in `_`-prefix system-name set

**Decision**: Mirror the explicit list in
`docs/design/nsl_tooling_design.md §4.1` — *not* `nsl_lang.ebnf
§15` and *not* the broader `pp.ebnf §3.1` helper closed set
(which is for the preprocessor, not the NSL language).

**Set**:
- `support.function.system.nsl`: `_display`, `_monitor`, `_write`,
  `_finish`, `_stop`, `_readmemh`, `_readmemb`, `_init`, `_delay`
  (9 names).
- `support.variable.system.nsl`: `_random`, `_time` (2 names).

**Rationale**: `nsl_tooling_design.md §4.1` is the single
authority for highlighter token categories; the spec
(`nsl_lang.ebnf §15`) lists no `_`-prefix names because they are
recognised at the lexer N11 layer, not the keyword set. Helpers
from `pp.ebnf §3.1` (`_int`, `_pow`, `_sin`, …) live in the
preprocessor sub-grammar and never appear in NSL source — so they
are out of scope for the highlighter that runs on `.nsl` files.

**Alternatives considered**:
- **Use `include/nsl/Basic/HelperSet.def` as source of truth** —
  rejected: that file enumerates *preprocessor* helpers per
  `pp.ebnf §3.1`, which never appear in `.nsl` content (they are
  consumed by the preprocessor before `.nsl` reaches the
  highlighter). Including them in the highlighter would
  mis-colour helper names that happen to collide with NSL
  identifiers in `.nsl` source.
- **Wildcard `_[A-Za-z]+` matching** — rejected: per parser note
  N11, three classes of `_`-prefix names exist: built-ins (closed
  list), reserved-for-future, and user-defined. Wildcard matching
  loses the third class' un-coloured-identifier behaviour and
  breaks `nsl_tooling_design.md §4.1`'s explicit "TextMate leaves
  [unscoped identifiers] un-scoped" rule.

---

## 4. CI integration — which `scripts/ci.sh` stage?

**Decision**: Stage 3 (`unit & layer tests`), as a tooling-tests
sub-step that runs after compiler unit / layer tests.

**Rationale**:
- Constitution Principle IX freezes the 6 stages by name and
  forbids dropping or renumbering. Tooling-track tests are not
  named in the Principle IX list because Principle IX was written
  describing the compiler track. Adding a 7th stage requires a
  constitutional amendment; running tooling tests inside an
  existing stage does not.
- Stage 3 is the natural fit: TextMate scope tests are a **layer
  test** (one layer of tooling — the highlighter — tested in
  isolation), parallel to lexer / parser / sema / dialect layer
  tests on the compiler side.
- Stage 3 already runs lit + gtest + nsl-opt for compiler layers;
  adding `vscode-tmgrammar-test` is a one-line dispatch addition
  in `./scripts/ci.sh`'s stage-3 handler.
- Failure mode: if Node is unavailable in the CI runner, the
  tooling-tests sub-step exits with a clear "Node not found" error;
  it does not silently skip. The compiler-layer tests in stage 3
  are unaffected.

**Alternatives considered**:
- **New stage 7 (`tooling-tests`)** — rejected: requires
  constitutional amendment per Principle IX's "Additional
  platforms MAY be added; none MAY be dropped without a
  constitutional amendment" — and adding a stage is a similar
  governance burden. Defer until more tooling-track milestones
  (T2-T12) need the cell — *then* an amendment makes sense; for
  T1 alone, the existing stage-3 home is correct.
- **Stage 2 (`static-checks`)** — rejected: scope tests are
  behavioural verification (the grammar, applied to fixtures,
  produces specific scopes), not static analysis (clang-format /
  clang-tidy / SPDX presence). Mis-classifying them in stage 2
  conflates "lint" with "behaviour-test".
- **Stage 5 (`end-to-end tests`)** — rejected: stage 5 is
  reserved for the audited-corpus simulation comparisons (Verilog
  + Icarus / Verilator), per Principle VI's named end-to-end
  drivers. TextMate scope tests are not end-to-end of the
  compiler.

---

## 5. Grammar file layout — `grammars/textmate/` vs `editors/vscode/syntaxes/`

**Decision**: Canonical artefact lives at
`grammars/textmate/nsl.tmLanguage.json`; **symlink** at
`editors/vscode/syntaxes/nsl.tmLanguage.json` points to it.
Build-step fallback materialises a copy in non-symlink
environments.

**Rationale**:
- `nsl_tooling_design.md §8` shared directory layout names
  `grammars/textmate/` as the canonical home — that is the
  source-of-truth location for cross-editor consumption (Sublime,
  Atom, GitHub web). VS Code's `package.json` schema requires the
  grammar path to be inside the extension folder
  (`editors/vscode/`) — hence the second copy.
- A symlink is the minimal divergence: one canonical file, one
  pointer. Git tracks symlinks portably.
- Symlinks fail on Windows zip-archive extraction (no symlink
  support without admin privileges). The build-step fallback
  resolves this: a `cmake -P` script (or a Python equivalent run
  by `./scripts/ci.sh`) detects whether the path is a symlink and
  if not, copies the file. Idempotent and cheap (≤ 50 KB).
- The CI matrix runs Linux only (Constitution Principle IX) so
  the symlink works in CI without the fallback. The fallback is
  for local dev on Windows hosts and downstream consumers who
  zip-extract a release tarball.

**Alternatives considered**:
- **Single canonical at `editors/vscode/syntaxes/`, no top-level
  `grammars/`** — rejected: violates `nsl_tooling_design.md §8`
  layout. Cross-editor consumers (Sublime, Atom) would have to
  reach inside `editors/vscode/` for their grammar — couples
  consumers to VS Code's directory shape.
- **Two independently-maintained copies, with a CI consistency
  check** — rejected: doubles the surface area for keyword drift
  bugs. The single-source-of-truth invariant from `KeywordSet.def`
  consumption already constrains the canonical artefact; adding a
  second hand-edit point reintroduces the drift the X-macro
  approach was designed to prevent.
- **Build-time generated copy only (no symlink)** — rejected: a
  source-tree symlink is more obvious to readers than a
  build-step output. CI fallback is fine; symlink-when-possible
  is the minimum-surprise default.

---

## 6. Grammar generation strategy

**Decision**: Hybrid generated-plus-hand-authored. The keyword
block (the largest mechanically-tracked surface) is generated by
`scripts/gen_textmate_grammar.py` from `include/nsl/Lex/KeywordSet.def`
plus an inline category-mapping table. Operators, comments,
literals, preprocessor directives, and `%IDENT%` are inline JSON
templates inside the generator. Output is a single
`grammars/textmate/nsl.tmLanguage.json` file checked into git.

**Rationale**:
- `KeywordSet.def` is **already the source of truth** for the
  lexer (`lib/Lex/KeywordSet.cpp` reads it via X-macro), the
  per-keyword fixture corpus (`scripts/gen_keyword_fixtures.py`
  reads it), and the TokenKind enum (`include/nsl/Lex/Token.h`).
  Reading it from a fourth consumer (T1) is the constitutionally-
  correct pattern (Principle I monotonic numbering — the keyword
  set is enumerated in one place; everything else mirrors it).
- The category-mapping table (which keyword maps to which
  TextMate scope sub-name) is a hand-authored `.toml` or inline
  Python dict — the mapping reflects the design decisions of
  `nsl_tooling_design.md §4.1`, which is not the same shape as
  the lexer's TokenKind enum.
- Operators, literals, and directives are not mechanically
  enumerable from a single `.def` file — they are pattern-shape
  declarations with regex-craftsmanship inside (e.g. the
  Verilog-sized literal regex includes `Z`/`X`/`U` markers and
  underscore separators). Handwriting them inside the
  generator's JSON-template literals keeps the regex craftsmanship
  reviewable.
- Generated output is **checked in** (not regenerated at build
  time) so the canonical artefact is reviewable in a PR diff,
  matching the precedent set by `test/lex/keywords/*.test`
  (also generator-output, also checked in).
- CI guard: a `./scripts/ci.sh stage-2` (static-checks) sub-step
  re-runs `gen_textmate_grammar.py` and `git diff --exit-code` on
  the canonical artefact; a divergent generator output fails the
  PR. This makes drift between `KeywordSet.def` and the grammar
  mechanically impossible.

**Alternatives considered**:
- **Fully hand-authored grammar** — rejected: each `KeywordSet.def`
  edit requires a parallel hand-edit in the grammar JSON; drift
  is a question of when, not whether. Defeats the X-macro pattern's
  raison d'être.
- **Fully generated grammar with externalised category-mapping
  config** — rejected: the category-mapping table is small (50ish
  lines) and reviewer-targeted; externalising it adds a third
  file with no clarity gain.
- **Generated at build time, not checked in** — rejected: no PR
  diff for grammar changes; reviewers cannot inspect what's
  shipping. Same anti-pattern as auto-formatted-but-not-committed
  files.

---

## 7. `package.json` for the VS Code extension shell

**Decision**: A minimal `package.json` with only the fields VS
Code requires to recognise the extension and load the grammar +
language config. **No** Marketplace metadata (publisher,
keywords, marketplace icons, sponsor URL). **No** dependencies.

**Required fields**:
```json
{
  "_comment_top": "SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception. ...",
  "name": "nsl",
  "displayName": "NSL Language Support",
  "description": "Syntax highlighting and editor configuration for NSL (Next Synthesis Language). T1 deliverable; LSP / lint / format land at T3+.",
  "version": "0.1.0",
  "engines": { "vscode": "^1.70.0" },
  "categories": ["Programming Languages"],
  "contributes": {
    "languages": [{
      "id": "nsl",
      "aliases": ["NSL", "nsl"],
      "extensions": [".nsl", ".nslh", ".inc"],
      "configuration": "./language-configuration.json"
    }],
    "grammars": [{
      "language": "nsl",
      "scopeName": "source.nsl",
      "path": "./syntaxes/nsl.tmLanguage.json"
    }]
  }
}
```

**Rationale**:
- These are the minimum fields for VS Code's
  `contributes.languages` + `contributes.grammars` to activate.
  Marketplace metadata is intentionally absent because T1 does
  NOT publish to the Marketplace (FR-022, deferred).
- Version `0.1.0` signals "pre-1.0; tooling-track is in milestone
  delivery"; bumps with each later T-track milestone that
  modifies VS Code-visible behaviour.
- `engines.vscode ^1.70.0` matches the VS Code release line that
  ships current TextMate grammar engine semantics; T1 does not
  use any field gated on a newer VS Code.

**Alternatives considered**:
- **Skip `package.json` entirely; just ship the JSON files** —
  rejected: VS Code does not auto-discover grammars from a
  filesystem path without a manifest; the user-experience
  acceptance test in User Story 2 (drop-folder install) requires
  `package.json`.
- **Marketplace-ready `package.json` with publisher / icon /
  README** — rejected: publication is FR-022 deferred; bundling
  marketplace metadata invites accidental publication and would
  need maintenance until T12 lifts the deferral.

---

## 8. Language-configuration field set

**Decision**: minimum-correct VS Code language-configuration:

```json
{
  "_comment_top": "SPDX-License-Identifier: ...",
  "comments": {
    "lineComment": "//",
    "blockComment": ["/*", "*/"]
  },
  "brackets": [
    ["{", "}"], ["[", "]"], ["(", ")"]
  ],
  "autoClosingPairs": [
    { "open": "{", "close": "}" },
    { "open": "[", "close": "]" },
    { "open": "(", "close": ")" },
    { "open": "\"", "close": "\"", "notIn": ["string", "comment"] }
  ],
  "surroundingPairs": [
    ["{", "}"], ["[", "]"], ["(", ")"], ["\"", "\""]
  ],
  "wordPattern": "(-?\\d*\\.\\d\\w*)|([^\\`\\~\\!\\@\\#\\%\\^\\&\\*\\(\\)\\-\\=\\+\\[\\{\\]\\}\\\\\\|\\;\\:\\'\\\"\\,\\.\\<\\>\\/\\?\\s]+)",
  "indentationRules": {
    "increaseIndentPattern": "^.*\\{[^}\"']*$",
    "decreaseIndentPattern": "^\\s*\\}"
  }
}
```

**Single-quote (`'`) is intentionally NOT auto-closed** because
NSL's Verilog-sized literal `8'hFF` would auto-insert a closing
`'`, mangling the literal. This is documented in the spec's Edge
Cases section and in the `language-config.contract.md` field
notes.

**Rationale**: each field maps to FR-012, FR-013, FR-014, FR-015.
The `wordPattern` regex follows VS Code's recommended form for
identifier-with-numeric-suffix languages.

**Alternatives considered**:
- **Auto-close single quote** — rejected: breaks Verilog-sized
  literals; the loss-of-fidelity exceeds the convenience gain.
- **Add `onEnterRules`** for nested-brace smart indentation —
  deferred: VS Code's default `indentationRules` already handles
  the `{` … `}` case correctly; richer rules belong with T5
  (LSP `formatting`) where the formatter has structural context.

---

## 9. Identifier "approximate count" in spec

**Observation**: the spec's FR-001 says "approximately 50 words"
and SC-002 says "≥ 50 distinct keyword assertions"; the actual
count from `KeywordSet.def` (and therefore `nsl_lang.ebnf §15`)
is **42** as of 2026-05-04 (31 Appendix-3 + 11 practical
additions).

**Decision**: keep the spec wording intentionally non-numeric
("every reserved keyword from `nsl_lang.ebnf §15`"); the
numeric estimates serve as illustrative bounds. Do NOT amend
the spec to read "exactly 42" — that number drifts with each
spec amendment under Principle I. The contract-side authority
is `KeywordSet.def`'s line count.

**Implementation**: `scripts/gen_textmate_fixtures.py` reads
`KeywordSet.def`, asserts it produces ≥ 1 fixture line per
keyword, and emits the actual count to its log. CI prints the
count; PR reviewers see drift mechanically.

---

## 10. Out-of-scope confirmations (re-stated for the record)

The following explicitly-deferred items remain out of scope for
T1 PR-landing and require no plan artefact:

- **`github-linguist/linguist` PR** — FR-022; deferred per
  `README.md §Roadmap` cross-org publication note.
- **VS Code Marketplace publication** — FR-022; deferred to T12
  which is itself deferred.
- **Tree-sitter grammar** — FR-021 (best-effort note); landed at T8.
- **LSP `semanticTokens`** — FR-020; landed at T4.
- **Vim native syntax / Emacs `nsl-mode.el`** — neither shipped
  at T1 (they are TextMate-incompatible formats); landed at T11
  per `nsl_tooling_design.md §4.4`.
- **Lint W/S/H rules** — out of T1; T6/T7.
- **Formatter** — out of T1; T2.
- **`#`/`&`/`|`/`^` context disambiguation** — best-effort at T1;
  perfected at T8 per FR-021.
