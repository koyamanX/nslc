<!-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception -->
# Data Model: clang-tidy Cleanup

**Scope**: Phase 1 design output for `004-clang-tidy-cleanup`. The "data
model" for this feature is project-policy artifacts: the `.clang-tidy`
config schema, the `.specify/memory/constitution.md` transitional-clause
removal, and the per-commit metadata required by FR-011.

This feature has **no source-code data model** in the C++ sense — it
edits existing M0/M1/M3 entities in place without adding new types.
Hence the entities below are configuration / governance / process
artifacts, not classes.

## Entity 1 — `.clang-tidy` config (schema after cleanup)

The repo-root `.clang-tidy` file is the project-policy artifact that
encodes the per-category dispositions chosen in `research.md` §1.

**Schema** (YAML — a clang-tidy native format):

```yaml
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#
# Project-wide clang-tidy policy for nslc. Written by feature 004
# (specs/004-clang-tidy-cleanup/) on 2026-04-27. Categories are
# either FIXED (no entries below — they passed the gate) or
# SUPPRESSED (with one-line rationale).
#
# Adding a new check: add it to `Checks:`, run the static-checks
# stage, fix every site, then merge. Do NOT add a check that
# would re-introduce known-suppressed categories.

Checks: >
  *,
  -<each-suppressed-category-from-research-§1>,
  ...

WarningsAsErrors: '*'

HeaderFilterRegex: '^(include|lib|tools|test_unit)/.*'

# === Suppression rationale (FR-009: rationale lives here, not in source) ===
#
# misc-non-private-member-variables-in-classes:
#   POD struct types (Diagnostic, Token, MacroDef, Frame) are an
#   established M1 pattern. Refactor to private+accessor is a
#   separate Principle II layer-design feature.
#
# cppcoreguidelines-avoid-const-or-ref-data-members:
#   By-ref dependency injection is the established M1 pattern
#   (MacroExpander holds MacroTable&, DiagnosticEngine&). Pointer
#   storage would obscure non-null contracts.
#
# misc-no-recursion:
#   Recursive descent is the intended shape for the parser /
#   preprocessor. Depth bounds (kMaxIncludeDepth,
#   kMaxExpansionDepth) enforce termination.
#
# readability-function-cognitive-complexity:
#   Cognitive complexity in parser code reflects grammar shape,
#   not unmaintainability. Splitting into helpers would hide
#   complexity, not reduce it.
#
# cppcoreguidelines-avoid-do-while:
#   do-while loop body executed-at-least-once semantics is
#   intended in lexer/keyword-set paths.
```

**Validation rules**:

- The file must be parseable by clang-tidy 18 (the toolchain
  pinned in `ghcr.io/koyamanx/nsl-nslc:dev`).
- The `Checks:` line must NOT include any category named in the
  rationale block (otherwise the suppression is contradicted).
- Every category in the rationale block must have a concrete
  one-paragraph justification (no `// TODO` or `// see issue #`
  placeholders per FR-009).
- `WarningsAsErrors: '*'` is mandatory — it is the regression-
  prevention mechanism per `research.md §2`.
- `HeaderFilterRegex` must include all four directories that
  ship code: `include/`, `lib/`, `tools/`, `test_unit/`.

**State transitions**: the file moves from "implicit defaults"
(today, no config) → "explicit allow-list with suppressions"
(after this feature). No further transitions planned in this
feature.

## Entity 2 — Constitution Principle IX section (post-cleanup)

The Constitution at `.specify/memory/constitution.md` has a
Principle IX section that currently carries a "transitional
clause" permitting merges while CI is incomplete. After the
gate is durably green, the transitional clause is removed.

**Pre-cleanup shape** (today, before this feature):

```text
### Principle IX — Continuous Integration & Delivery

[steady-state rule text]

**Transitional clause.** Until the CI pipeline is online,
every PR ... [transitional rule text].
```

**Post-cleanup shape** (after the close-out commit):

```text
### Principle IX — Continuous Integration & Delivery

[steady-state rule text — unchanged]
```

**Validation rules** (FR-005 / SC-004):

- A `grep -i "transitional" .specify/memory/constitution.md`
  must return zero matches inside the Principle IX section.
- The steady-state rule text must remain BYTE-IDENTICAL to its
  pre-cleanup form. The close-out commit is a pure deletion of
  the transitional paragraph + its surrounding quote markers.
- The "Pipeline stages" and "Governance" sections that
  reference "the transitional clause" must be edited in lockstep
  to remove those references (otherwise grep finds dangling
  pointers to a clause that no longer exists).

**State transitions**:

```text
[transitional active] ──(close-out commit)──▶ [steady-state]
```

One-way; no rollback path is contemplated. If the gate goes red
again post-merge, the fix is a forward-fix PR, not a constitution
revert.

## Entity 3 — Per-commit metadata (FR-011)

Every per-category cleanup commit on the feature branch carries a
structured commit-message body that provides the bisect-friendly
audit trail SC-005 requires.

**Schema**:

```text
<type>(<scope>): T<NNN> — <category> cleanup (<count> sites, <files> files)

<one-paragraph context: why this category, what the fix shape is>

Categories cleared in this commit:
- <category-name>: <count> warnings (was <previous>)

Verification inside ghcr.io/koyamanx/nsl-nslc:dev:
- static-checks delta: <previous-total> → <new-total> warnings
- lit: <count>/<count> pass (no regression)
- ctest: <count>/<count> pass (no regression)

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
```

**Validation rules** (FR-011):

- `<type>` is one of `chore`, `style`, `refactor` per the project's
  conventional-commit style (see existing M0/M1/M3 commit history
  on master).
- `<scope>` is the directory under which the bulk of the edits
  land, e.g., `include`, `lib/preprocess`, `lib/lex`. For
  cross-cutting categories (clang-format sweep), use `tree`.
- `<category>` is the literal clang-tidy category name (e.g.,
  `misc-const-correctness`).
- `<count>` is the number of warning sites the commit clears.
  Computed by: pre-commit `static-checks` total − post-commit
  `static-checks` total.
- `<files>` is the count of files this commit modifies (from
  `git diff --stat HEAD~1 HEAD | tail -1`).

**State transitions** (per commit):

```text
[pre-commit: tree dirty]
   │
   ├──▶ run clang-tidy --fix on category X
   ├──▶ run clang-format -i on touched files
   ├──▶ run full lit + ctest inside container; observe GREEN
   ├──▶ stage files; verify SPDX intact (FR-008)
   └──▶ commit with metadata schema above
[post-commit: tree clean, count reduced]
```

## Entity 4 — Suppression site marker (per-site `// NOLINT(...)`)

When a category is BROADLY enabled but a SINGLE site genuinely
warrants exemption, the project allows a per-site `// NOLINT(...)`
comment with a justification. This is the FR-009 escape hatch.

**Schema**:

```cpp
// NOLINTNEXTLINE(<category>) <one-line rationale>
<the-line-being-suppressed>
```

**Validation rules**:

- Must use `// NOLINTNEXTLINE` (single-line scope), NOT
  `// NOLINT` (current-line scope is ambiguous in editor render).
- `<category>` is the exact clang-tidy category name being
  suppressed (e.g., `cppcoreguidelines-pro-bounds-constant-array-index`).
- `<one-line rationale>` is mandatory and must be project-meaningful,
  not "fix later" or "TODO" (FR-009).
- A `// NOLINT*` line without a parenthesized category name is
  rejected by review (it would silence ALL categories for that
  line, undermining FR-006's regression-prevention guarantee).
- A `// NOLINT*` line is allowed in source code only if the
  rationale could not reasonably move into `.clang-tidy`'s global
  rationale block — i.e., the suppression is genuinely site-local.

**State transitions**: per-site markers are stable; they live
forever unless the underlying code is rewritten in a future PR.

## §5 Cross-entity invariants

- **`.clang-tidy` rationale block (Entity 1) ↔ `// NOLINTNEXTLINE`
  comments (Entity 4)**: a category is either globally suppressed
  (Entity 1) OR locally suppressed at specific sites (Entity 4),
  never both. Globally-suppressed categories MUST NOT appear in
  any `// NOLINTNEXTLINE` comment.
- **Constitution edit (Entity 2) ↔ commit graph (Entity 3)**: the
  constitution close-out commit is the LAST commit on the feature
  branch, and its `static-checks` delta is `0 → 0` (the gate is
  already green by that point; the commit only edits a doc).
- **Commit metadata (Entity 3) ↔ FR-011**: every cleanup commit
  honors the schema, including the constitution close-out commit
  (whose `<count>` is 0).

All invariants are testable by `nsl-coupling-audit` and
`nsl-constitution-review` agents pre-merge.
