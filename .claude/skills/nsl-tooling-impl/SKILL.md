---
name: "nsl-tooling-impl"
description: "Implement nsl-lsp / nsl-fmt / nsl-lint and editor-grammar artifacts (TextMate, tree-sitter); reuses libNSLFrontend.a — covers T1–T12."
argument-hint: "Tool feature (e.g., 'add textDocument/hover to nsl-lsp')"
metadata:
  author: "nslc-project"
user-invocable: true
disable-model-invocation: false
---

## User Input

```text
$ARGUMENTS
```

You **MUST** consider the user input before proceeding (if not empty).

## Role

Owns the four developer-tooling deliverables that share the same `libNSLFrontend.a` as the compiler driver — Constitution **Principle II** prohibits any duplication of lex/parse/sema in tooling.

| Tool | Milestones | Scope |
|---|---|---|
| `nsl-lsp` | T3, T4, T5, T9, T10 | Language Server (clangd-style, `TUScheduler`-based) |
| `nsl-fmt` | T2, T5 | Wadler-Leijen pretty printer + NSL-specific rules |
| `nsl-lint` | T6, T7 | W (warnings) / S (semantic) / H (hardware) rule tiers |
| Highlighter | T1, T8, T11 | TextMate + tree-sitter grammars; editor packages |

The full T-track table is in `README.md` §Roadmap. Inverse roll-ups (LSP method → milestone, lint rule → milestone, etc.) are in `CLAUDE.md` (root) §2.

## Outline

1. **Locate the feature.** Cross-reference:
   - `CLAUDE.md` (root) §2.1–§2.5 — which milestone delivers the feature you're working on
   - `docs/design/nsl_tooling_design.md` per `docs/CLAUDE.md` §3 task-map (LSP → §3, lint → §6, formatter → §5, highlighter → §4)

2. **TDD entry (Principle VIII).** Hand off to `/nsl-test-author`:
   - **LSP method** — one LSP-protocol test per method against a fixture document
   - **Lint rule (W/S/H)** — one passing case + one failing case + suggested-fix verification per rule
   - **Formatter rule** — round-trip test (`format → format == noop`); golden-output test
   - **Highlighter** — TextMate scope test on a fixture; tree-sitter parse-tree golden

3. **Implement against `libNSLFrontend.a` (Principle II).**
   - **No duplicated lex/parse/sema in tooling code.** All tools link `libNSLFrontend.a`; if the front-end is missing what you need, extend the front-end, don't reimplement it.
   - Files:
     - `nsl-lsp` → `include/nsl/Tools/LSP/`, `lib/Tools/LSP/`, `tools/nsl-lsp/main.cpp`
     - `nsl-fmt` → `include/nsl/Tools/Fmt/`, `lib/Tools/Fmt/`, `tools/nsl-fmt/main.cpp`
     - `nsl-lint` → `include/nsl/Tools/Lint/`, `lib/Tools/Lint/`, `tools/nsl-lint/main.cpp`
     - Editor grammars → `editors/textmate/`, `editors/tree-sitter/`, `editors/<editor>/`

4. **LSP-specific rules.**
   - clangd-style three-layer architecture (`docs/design/nsl_tooling_design.md` §3.1, lines **105–148**)
   - `TUScheduler`-based incremental document management (§3.3, **170–201**)
   - CST + incremental parse from shared infrastructure (§2, **48–104**)
   - Stable IDs for cross-reference / rename / definition

5. **Formatter-specific rules.**
   - Wadler-Leijen pretty printer + Doc IR (§5.2, **598–664**)
   - NSL-specific rules (§5.3, **666–701**) — `proc`/`state` indentation, `par` braces, etc.
   - Walks the CST layer (§2, **80–104**) so it preserves `#line` directives across format
   - LSP integration delegates to the `nsl-fmt` library (T5)

6. **Lint-rule-specific (W/S/H tiers).**
   - **W** — grammar-level warnings (T6): unreachable `state`, unused decl, `function` vs `func`, etc.
   - **S** — semantic / style (T6, with CFG-required ones at T7)
   - **H** — hardware-design rules (T7): missing reset, comb loop, multi-driver, etc.
   - Rule interface: `docs/design/nsl_tooling_design.md` §6.3 (**792–835**)
   - Configuration: TOML format (§6.5, **861–879**)
   - **Adding a rule post-T7 is a routine PR** — write the fixture tests first, observe failing, then implement. The `CLAUDE.md` §2.2 lint-rules table MUST gain a row in the same change.

7. **Highlighter-specific.**
   - TextMate (T1): `editors/textmate/syntaxes/nsl.tmLanguage.json` + `language-configuration.json`
   - Tree-sitter (T8): `editors/tree-sitter/grammar.js` + `queries/highlights.scm`
   - **Keyword list MUST match `docs/spec/nsl_lang.ebnf` §15** (lines **783–824**) — drift is a Principle VII violation
   - GitHub `linguist` PR — **deferred** (see `README.md` §Tooling-track note); ship the in-tree artifacts only

8. **Roll-up update (Principle VII).** When you add a new LSP method, lint rule, formatter capability, highlighter scope, or editor target, update the relevant sub-table in `CLAUDE.md` (root) §2 in the same PR.

9. **Verify.** Confirm:
   - [ ] TDD: test was observed failing first
   - [ ] No duplicated lex/parse/sema in the tool (Principle II)
   - [ ] If a new lint rule: added under correct W/S/H tier with pass + fail + fix-it fixture
   - [ ] If a highlighter change: keyword list still matches `nsl_lang.ebnf` §15
   - [ ] Roll-up table in `CLAUDE.md` §2 updated in the same PR
   - [ ] SPDX header on every new file

## Constitutional anchors

- **Principle II** — Layered Library Architecture (NO duplicated front-end code)
- **Principle VI** — Layered Test Discipline (per-rule fixtures)
- **Principle VII** — Spec ↔ Design Coupling (highlighter keyword list; roll-up tables)
- **Principle VIII** — Test-First Development (NON-NEGOTIABLE)
