<!-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception -->

# Contract — VS Code Extension Shell

**Feature**: `010-t8-tree-sitter-grammar`
**Phase**: 1 (design / contracts)
**Date**: 2026-05-05

This contract freezes the **VS Code extension activation
surface and provider registration** that T8 ships under
`editors/vscode/treesitter/`. It corresponds to FR-013 and
US3.

User Story 3's "extension-host load without error"
acceptance bar (AS3.1) is the dynamic enforcement of this
contract; per spec.md US3 it is a manual smoke-test in VS
Code rather than a CI assertion (VS Code's
extension-host launcher is not headless-friendly).

---

## 1. Manifest fields (`editors/vscode/package.json` AMENDMENTS)

T1's existing `editors/vscode/package.json` registers the
`nsl` language ID and the TextMate contribution. T8 adds:

| Field | Required value | Notes |
|---|---|---|
| `engines.vscode` | `"^1.84.0"` (or whatever T1 already pins; do NOT downgrade) | T8 uses `DocumentSemanticTokensProvider` which has been stable since 1.84 |
| `activationEvents` | MUST contain `"onLanguage:nsl"` | the existing T1 entry covers this; verify present |
| `main` | `"./treesitter/extension.js"` (compiled from `extension.ts`) | T1 has no `main` field; T8 adds one |
| `contributes.semanticTokenTypes` | array of **21** entries matching highlights.scm captures (§1.4 of data-model.md PLUS the FR-009 control-terminal capture per `highlights-coverage.contract.md` §2) | one entry per capture name. The +1 deviation from the §1.4 minimum is `variable.builtin.terminal`, locked by the T019 fixture |
| `contributes.semanticTokenScopes` | array mapping each token-type to a TextMate-style scope name | for theme fallback when no semantic-token rule is defined |
| `dependencies.web-tree-sitter` | pinned to a specific patch version compatible with `tree-sitter-cli@0.22.x` ABI | research.md §1 / §7 |

---

## 2. Activation surface (`editors/vscode/treesitter/extension.ts`)

The extension's `activate()` function MUST:

1. **Initialise `web-tree-sitter`**: call
   `Parser.init({locateFile: …})` pointing at the
   `tree-sitter-nsl.wasm` file shipped alongside the
   extension (consumer downloads from CI workflow artefact
   or tagged release per Q2 → Option C).
2. **Load the WASM grammar**: `await
   Language.load(path.join(extensionPath,
   'tree-sitter-nsl.wasm'))`.
3. **Register the semantic-tokens provider** for the `nsl`
   language ID via
   `vscode.languages.registerDocumentSemanticTokensProvider`.
4. **Register a dispose handler** so the parser cleanly
   tears down when the extension deactivates.

The extension MUST NOT:

- Initialise eagerly on `vscode-startup` (per
  `activationEvents` `onLanguage:nsl`).
- Spawn external processes (no LSP client; no terminal).
- Modify settings, themes, or other extensions.
- Register diagnostic providers (those are T3 / LSP
  territory).

When `tree-sitter-nsl.wasm` is **missing** from the
extension folder (consumer hasn't downloaded the workflow
artefact yet), the extension MUST log a clear
`vscode.window.showWarningMessage` directing the consumer to
download the WASM (see quickstart.md), then exit
`activate()` cleanly without throwing. AS3.3's "graceful
degradation to T1 levels" is what this code path delivers.

---

## 3. Provider implementation
(`editors/vscode/treesitter/highlight-provider.ts`)

The `DocumentSemanticTokensProvider`'s
`provideDocumentSemanticTokens(document, ct)` method MUST:

1. Parse `document.getText()` with the cached
   `Parser` instance and the loaded NSL `Language`.
2. Run the tree-sitter query from `highlights.scm` against
   the parse tree.
3. Convert each query capture into a
   `vscode.SemanticTokens` entry mapping
   (line, column, length, tokenTypeIndex, tokenModifiersBitmap).
4. Return the `SemanticTokens` builder result.

The provider MUST handle:

- **Document changes**: VS Code calls
  `provideDocumentSemanticTokens` after every edit.
  Tree-sitter's incremental parse is used (the parser
  caches the previous tree per-document).
- **Cancellation**: respect the `CancellationToken`; abort
  if cancelled.
- **`(ERROR)` nodes**: don't crash; emit captures for the
  nodes that DID parse successfully and skip the error
  subtree.

The `SemanticTokensLegend` MUST contain **21** entries —
the 20 from data-model.md §1.4 (captures #1–#20) plus the
FR-009 control-terminal capture (#21,
`variable.builtin.terminal`, per
`highlights-coverage.contract.md` §2). The order is fixed:
#1–#20 in source order followed by #21. The legend's index
= the `tokenType` parameter in `pushLegend()` calls.

---

## 4. Coexistence with T1

T8's semantic-tokens contribution composes with T1's
TextMate contribution per VS Code's documented precedence:

1. **TextMate (T1)** — base layer; assigns scopes to every
   token using regex.
2. **Semantic tokens (T8)** — overrides where present.
   Identifiers that T1 left un-scoped get T8's
   `@variable.register` / `@variable.wire` / etc.
3. **LSP `semanticTokens` (T4, future)** — overrides T8
   when the LSP server is connected. T4's wider symbol-table
   knowledge produces refinement T8 cannot (e.g. shadowing,
   cross-file resolution).

T8's extension MUST NOT:

- Disable T1's TextMate contribution.
- Override scopes T1 already assigns (keywords, operators,
  literals, comments, strings, preprocessor directives).
  T8's job is *supplementing* T1, not replacing it.

---

## 5. Test surface

| Acceptance scenario | Test mechanism |
|---|---|
| AS3.1 (extension loads without error) | manual VS Code launch + check the developer-tools console for extension-host errors; recorded in PR description for the T8 implementation PR |
| AS3.2 (T1 base + T8 override) | manual VS Code session against `examples/03_register.nsl`; observe register and wire references colour distinctly via T8 while keywords retain T1's colour; recorded in PR description |
| AS3.3 (graceful degradation when uninstalled) | manual VS Code session with extension uninstalled; observe T1 levels of colouring; recorded in PR description |
| AS3.4 (WASM byte-identity) | CI's `treesitter-wasm-determinism` sub-step; automated |

All four are attestation-via-PR-description (manual smoke
tests), with AS3.4 the only one that is a CI assertion.
This is a deliberate trade-off: VS Code's extension-host
runner is not designed for headless CI, and the value-add
of automating extension-load assertions in CI is small
relative to the maintenance cost of a VS Code-headless
fixture.

---

## 6. Out of scope (boundaries)

T8's extension MUST NOT include any of the following — they
are explicitly assigned to other milestones:

- LSP client / `nsl-lsp` integration → **T3**
- LSP-based hover, definition, references, refactor → **T3+/T4/T9**
- Diagnostics → **T3**
- Inlay hints (bit-width annotations) → **T10**
- Linting (W/S/H rule UI integration) → **T6/T7**
- Format-on-save / format-on-type → **T2/T5**
- Marketplace publishing → deferred per `README.md §Roadmap` T1/T12 deferral note
