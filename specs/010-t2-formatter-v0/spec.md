# Feature Specification: T2 — Formatter v0 (`nsl-fmt`)

**Feature Branch**: `010-t2-formatter-v0`
**Created**: 2026-05-04
**Status**: Draft
**Input**: User description: "T2"

> **What this feature is.** The first installment of the NSL code
> formatter — a `gofmt`/`rustfmt`/`black`-style canonical
> pretty-printer for NSL source. Per project root
> [`CLAUDE.md`](../../CLAUDE.md) §2.3, T2's deliverable is exactly
> "Formatter v0: indent, brace style, operator spacing, NSL-specific
> rules ([`docs/design/nsl_tooling_design.md`](../../docs/design/nsl_tooling_design.md)
> §5.3) + CLI + `--check`." The end-state for this milestone is a
> standalone CLI binary (`nsl-fmt`) plus a shareable library
> (`libNslFmt.a`) that T5 will later wire into the LSP's
> `textDocument/formatting` and `textDocument/rangeFormatting`
> handlers (T5 is **out of scope** for T2).

## Clarifications

### Session 2026-05-04

- Q: How should `nsl-fmt` handle preprocessor directives? → A: Format raw input *before* preprocessing — treat each directive line as an opaque CST token; recurse into NSL fragments only between directives (clang-format style).
- Q: Is `--range LINE:LINE` part of T2 or deferred to T5? → A: Ship `--range` at T2 (the layout engine already operates on subtrees; defining the full CLI surface now avoids retrofit at T5).
- Q: How should `nsl-fmt` handle errors when invoked on multiple files? → A: Continue past parse errors and check-mismatches; collect and report ALL offending files; exit non-zero if any failed (gofmt / black --check behavior).

### Session 2026-05-05

- Q: How should the formatter handle inputs that don't fully parse against the M1/M2 NSL grammar (BOM bytes, `%IDENT%` in identifier position, top-level system-task expressions)? → A: Strict refusal per FR-012 — any input the lex+parse pipeline rejects → exit non-zero. Tolerated cases are limited to those explicitly named in FR-012a (directive lines + `%IDENT%` splices treated as opaque CST tokens). BOM at file start is NOT tolerated. Test fixtures depending on byte-passthrough of unparseable input must be rewritten with valid NSL.
- Q: When the formatter reformats code containing inline comments BETWEEN tokens of a single statement (e.g., `reg /* width 8 */ q[8];`), what happens to those comments? → A: Preserve inline byte-for-byte — keep the comment between the same two tokens. The formatter normalizes spacing AROUND the comment (one space before + one space after, unless suppressed). No hoisting to leading/trailing position; no refusal. This protects FR-008 idempotence.
- Q: What's the policy for the trailing newline at end of output? → A: Always emit exactly one trailing `\n` (gofmt / rustfmt / black convention). If input lacks one, formatter adds it. If input has multiple trailing blank lines, formatter normalizes to one. Idempotent by construction.

## User Scenarios & Testing *(mandatory)*

### User Story 1 — Canonical formatting on demand (Priority: P1)

An NSL author runs `nsl-fmt foo.nsl` (or `nsl-fmt -i foo.nsl`) and
gets back a file rewritten to one canonical style. Indent, brace
placement, operator spacing, `alt`/`any` case alignment, struct
member alignment, `proc_name` argument-list wrapping, and bit-slice/
concat spacing all match the project style without the author
hand-tweaking anything.

**Why this priority**: Removing bikeshedding from PR review is the
core value the formatter delivers; without canonical output there is
nothing to integrate with CI or with editors. P1 is the MVP — every
later story depends on this output being deterministic.

**Independent Test**: Author runs `nsl-fmt --stdin < malformatted.nsl
> out.nsl` and diffs `out.nsl` against the hand-curated golden file;
the diff is empty. Idempotence is verified by running `nsl-fmt`
twice — the second run produces no further changes.

**Acceptance Scenarios**:

1. **Given** a syntactically valid NSL file with non-canonical
   whitespace (mixed tabs/spaces, missing operator spacing,
   misaligned `alt` arrows), **When** the user runs `nsl-fmt
   --stdin`, **Then** the output applies the canonical NSL-specific
   rules from [`nsl_tooling_design.md`](../../docs/design/nsl_tooling_design.md)
   §5.3 and the diff against the matching golden file is empty.
2. **Given** an already-canonically-formatted NSL file, **When** the
   user runs `nsl-fmt --stdin` against it, **Then** the output is
   byte-identical to the input (idempotence).
3. **Given** an NSL file with an attached trailing-line comment
   (`reg foo[8]; // 8-bit accumulator`) and a leading block comment
   above a declaration, **When** the user runs `nsl-fmt`, **Then**
   the trailing comment stays on the same line as the declaration
   and the leading block comment stays immediately above its
   declaration.
4. **Given** a file containing an `alt` block with three cases of
   varying condition width, **When** the user runs `nsl-fmt`,
   **Then** the `:` separators of all three cases align vertically
   (per §5.3 rule 1).
5. **Given** a `struct` block with mixed-length member names,
   **When** the user runs `nsl-fmt`, **Then** the `[N]` bit-width
   brackets align vertically (per §5.3 rule 2).

---

### User Story 2 — CI gate via `--check` (Priority: P2)

A CI maintainer adds a step that runs `nsl-fmt --check
$(git ls-files '*.nsl')`. If any file would be changed by the
formatter, the step fails with a non-zero exit code and prints a
unified diff of the would-be changes; if every file is already
canonical, it exits 0 silently.

**Why this priority**: P2 because it depends on P1 (the canonical
output), but is the mechanism that actually enforces project-wide
conformance. Without `--check`, the formatter is opt-in and drift
returns within a release cycle.

**Independent Test**: CI maintainer wires `nsl-fmt --check` into a
GitHub Actions step; deliberately commits a malformatted file; the
PR fails the step with the offending file's diff in the log. Reverts
the bad commit; the step passes.

**Acceptance Scenarios**:

1. **Given** a tree where every `.nsl` file is already canonically
   formatted, **When** the user runs `nsl-fmt --check file1.nsl
   file2.nsl`, **Then** the exit code is `0` and stdout/stderr are
   empty.
2. **Given** a tree where one of the supplied files would be
   modified by `nsl-fmt`, **When** the user runs `nsl-fmt --check
   file1.nsl file2.nsl`, **Then** the exit code is non-zero and a
   unified diff (file path + `---`/`+++` header + hunks) is printed
   for that file on stdout.
3. **Given** the user passes both `--check` and `-i`, **When**
   `nsl-fmt` starts, **Then** it refuses with a clear error message
   ("--check and --in-place are mutually exclusive") and exits with
   a non-zero code.

---

### User Story 3 — Library reuse for T5 LSP integration (Priority: P3)

A future tooling-track engineer (working on T5 — `textDocument/
formatting`) links `libNslFmt.a` from `nsl-lsp` and forwards LSP
formatting requests to a stable in-memory entry point that returns a
list of text edits. No reparse logic, layout engine, or rule
implementation is duplicated between `nsl-fmt` and `nsl-lsp`.

**Why this priority**: P3 because it gates T5, not T2 itself. The
T2 binary works without it — but if `libNslFmt.a` is missing, T5
must rebuild the entire formatter from scratch and Principle II
(no-duplication) is violated.

**Independent Test**: A throwaway link-test target depends on
`libNslFmt.a`, calls the public formatting entry point on a string
buffer, and asserts the returned text matches the CLI's output for
the same input.

**Acceptance Scenarios**:

1. **Given** the T2 build produces `libNslFmt.a` and a public
   header, **When** an external translation unit `#include`s the
   header and links the archive, **Then** it can format a `StringRef`
   to a `std::string` without invoking the CLI binary.
2. **Given** the same input, **When** the CLI runs `nsl-fmt --stdin`
   and the library entry point is invoked from a unit test, **Then**
   both produce byte-identical output.

---

### User Story 4 — Project-level style customization (Priority: P3)

A project lead places a `.nsl-fmt.toml` at the repo root with a
small number of toggles (indent width, brace style, alignment
on/off, etc., per [`nsl_tooling_design.md`](../../docs/design/nsl_tooling_design.md)
§5.1). Every invocation of `nsl-fmt` from inside the tree picks up
that file automatically; an explicit `--config PATH` overrides
discovery.

**Why this priority**: P3 because the canonical defaults serve the
common case; configuration is a convenience for projects that need
to deviate. Cannot be cut entirely because the design doc names
configurable knobs as part of the "opinionated, configurable"
contract.

**Independent Test**: Drop a `.nsl-fmt.toml` at the repo root that
sets `indent = 2`; run `nsl-fmt` on a file deeply nested under that
root; observe 2-space indentation. Pass `--config /dev/null` and
observe the default 4-space indentation.

**Acceptance Scenarios**:

1. **Given** a `.nsl-fmt.toml` at the repo root that sets `indent = 2`,
   **When** the user runs `nsl-fmt subdir/foo.nsl` from any
   subdirectory of the repo, **Then** the formatter walks upward
   from the input file to discover the config and emits 2-space
   indentation.
2. **Given** the user passes `--config path/to/custom.toml`, **When**
   `nsl-fmt` runs, **Then** the explicitly named file is the only
   configuration consulted (root-walk is suppressed).
3. **Given** `.nsl-fmt.toml` contains an unknown key, **When**
   `nsl-fmt` starts, **Then** it warns on stderr (naming the key
   and the line number) and proceeds with the unknown key ignored.

---

### Edge Cases

- **Parse-error input**: input that fails to lex or parse cannot be
  reformatted safely (a partial AST/CST cannot be re-emitted without
  risking semantic drift). The formatter MUST refuse: print the
  underlying lexer/parser diagnostic on stderr, leave the input
  file untouched (if `-i`), and exit with a non-zero code. This
  matches `gofmt`/`rustfmt`/`black`.
- **Files containing preprocessor directives** (`#include`,
  `#define`, `#ifdef`, `#line`, `%IDENT%`): the formatter parses
  the raw source *before* preprocessing. Each directive line is
  preserved as an opaque CST token (the formatter does NOT
  reformat the directive's payload, e.g. it does not normalize
  whitespace inside `#define FOO(x,y) ((x)+(y))`); NSL fragments
  *between* directives are recursed into and reformatted normally.
  This matches `clang-format`'s preprocessor model and ensures
  every audited corpus file (all of which use `#include`) survives
  the round-trip.
- **Well-formed NSL with lines exceeding `max_line_length` after
  formatting** (e.g., a 100-char `_display(...)` argument string
  inside a `func` body, with no break point): the formatter MUST
  emit the over-long line rather than corrupt the source by
  inserting line breaks at unsafe positions. This matches
  `rustfmt`'s "best effort" stance for un-breakable constructs.
  Note: the input must be well-formed NSL — top-level
  `_display(...)` outside any `func`/`proc` body is a parse error
  per FR-012 (clarified Session 2026-05-05).
- **Empty input** (zero-byte file or whitespace-only file): MUST
  succeed with exit code 0 and emit canonical output (empty file or
  trailing newline only, per project convention).
- **Mixed line endings** (CRLF in the middle of an LF file): MUST
  normalize to LF on output (the project is Linux-first per
  `ghcr.io/koyamanx/nsl-nslc:dev` build environment).
- **BOM at start of file**: NOT tolerated per FR-012 (clarified
  Session 2026-05-05). The lexer does not recognise a leading
  UTF-8 BOM byte sequence; a BOM-prefixed input is a parse error
  → refused. Users with BOM-bearing source must strip the BOM
  before formatting (e.g., `sed -i '1s/^\xef\xbb\xbf//' foo.nsl`).

## Requirements *(mandatory)*

### Functional Requirements

#### CLI surface (`nsl-fmt`)

- **FR-001**: `nsl-fmt` MUST accept one or more file paths as
  positional arguments. With no flags, it MUST print the formatted
  output of each file to stdout in input order.
- **FR-002**: `nsl-fmt` MUST support `-i` / `--in-place` to rewrite
  each named file with its formatted contents. The rewrite MUST be
  atomic (write-to-temp + rename) to avoid corrupting the source on
  crash.
- **FR-003**: `nsl-fmt` MUST support `-c` / `--check` to exit
  non-zero if any named file would be modified, printing a unified
  diff per offending file. With `--check` alone (no offending
  files), exit code MUST be 0 and stdout/stderr MUST be empty.
- **FR-003a**: When invoked on multiple positional file arguments,
  `nsl-fmt` MUST process every file regardless of per-file errors:
  parse errors, check-mismatches, and write failures on individual
  files MUST NOT abort the loop. After all files are processed,
  the formatter MUST exit `0` if every file succeeded, or non-zero
  if any file failed; the diagnostic for each failed file MUST be
  emitted on stderr (or, for `--check` mismatches, the diff on
  stdout) so a CI log surfaces the complete list in one run.
- **FR-004**: `nsl-fmt` MUST support `--stdin` to read source from
  standard input and write the formatted output to standard output
  (no file path needed; one input only).
- **FR-005**: `nsl-fmt` MUST support `--config PATH` to load a
  named configuration file instead of discovering one via root-walk
  (see FR-013).
- **FR-006**: `nsl-fmt` MUST exit non-zero with an explanatory
  error if `--check` and `-i` are combined, or if `--stdin` is
  combined with positional file arguments.
- **FR-007**: `nsl-fmt` MUST support `--range LINE:LINE` to format
  only the given line range (1-indexed, inclusive). Lines outside
  the range MUST be emitted byte-identical to the input. If the
  range falls partly outside the file, the formatter MUST exit
  non-zero with a clear diagnostic. `--range` MUST be combinable
  with `--stdin` and `--check`; it MUST NOT be combinable with
  multiple positional file arguments.

#### Output guarantees (canonical style)

- **FR-008**: For every accepted input, the formatter MUST be
  **idempotent**: a second invocation on its own output MUST produce
  byte-identical output.
- **FR-009**: The formatter MUST apply the six NSL-specific rules
  in [`nsl_tooling_design.md`](../../docs/design/nsl_tooling_design.md)
  §5.3 (alt/any case alignment, struct member alignment, proc_name
  argument-list wrapping, bit-slice/concat spacing, operator
  spacing, attached-comment preservation).
- **FR-010**: The formatter MUST preserve all comments (line and
  block) in their semantic position relative to the surrounding
  declaration, per §5.3 rule 6. **Inline comments between two
  tokens of a single statement** (clarified Session 2026-05-05)
  MUST be preserved byte-for-byte at the same token-relative
  position. The formatter MAY normalize whitespace AROUND the
  comment (one space before + one space after), but MUST NOT
  hoist the comment to a leading or trailing line position. This
  protects FR-008 idempotence.
- **FR-011**: The formatter MUST preserve numeric literal forms
  (decimal, hex `0x`, binary `0b`, NSL value literals with `Z`/`X`/
  `U`) byte-for-byte; it MUST NOT canonicalize between bases.
- **FR-012**: The formatter MUST refuse on parse error: print the
  diagnostic on stderr, leave any `-i` target untouched, and exit
  non-zero. No partial output is written. **"Parse error" means
  any input that the M1 lexer + M2 parser pipeline rejects**
  (clarified Session 2026-05-05). The only tolerated cases of
  pre-parse byte sequences are those explicitly named in FR-012a
  (preprocessor directive lines, `%IDENT%` splices) — which the
  directive-aware pre-pass handles before lex/parse runs. Any
  other byte sequence the lexer cannot tokenise (BOM, vendor
  pragmas, etc.) is a parse error.
- **FR-012a**: The formatter MUST parse raw source *before*
  preprocessing. It MUST emit a directive-aware CST that preserves
  each preprocessor directive line (`#include`, `#define`,
  `#undef`, `#ifdef`, `#ifndef`, `#if`, `#else`, `#endif`,
  `#line`, `%IDENT%` splices) as an opaque CST token whose payload
  is reproduced byte-for-byte on output. NSL fragments *between*
  directives MUST be parsed and reformatted normally. The
  formatter MUST NOT reorder, deduplicate, or syntactically
  rewrite directives.

#### Configuration

- **FR-013**: When `--config` is not given, `nsl-fmt` MUST walk
  upward from each input file (or, with `--stdin`, from the current
  working directory) looking for `.nsl-fmt.toml`. The first one
  found wins; if none is found, built-in defaults apply.
- **FR-014**: The configuration file MUST support at minimum the
  ten keys listed in [`nsl_tooling_design.md`](../../docs/design/nsl_tooling_design.md)
  §5.1: `indent`, `max_line_length`, `spaces_around_binary_ops`,
  `spaces_inside_braces`, `align_struct_members`,
  `align_case_arrows`, `brace_style`, `trailing_commas`,
  `blank_lines_between_modules`, `preserve_comments`. Built-in
  defaults MUST match the example values shown in §5.1.
- **FR-015**: Unknown configuration keys MUST emit a stderr warning
  (with key name and line number) but MUST NOT abort the run.
- **FR-016**: Out-of-range or wrong-type values for known keys
  (e.g., `indent = "potato"`) MUST emit a stderr error and abort
  with a non-zero exit code, since silently using a default would
  hide the user's mistake.

#### Library (`libNslFmt.a`)

- **FR-017**: The build MUST produce `libNslFmt.a` (or a static
  archive of equivalent name per the project's CMake conventions)
  that exposes a stable C++ entry point taking a source `StringRef`
  and a configuration object, returning either the formatted string
  or a diagnostic on failure.
- **FR-018**: The library MUST NOT duplicate any reusable code
  from `libNSLFrontend.a` (Principle II — no-duplication rule).
  Specifically, it MUST share the existing token / keyword tables,
  source-location infrastructure, and `DiagnosticEngine`. Because
  `libNslFmt.a` parses *raw* source (pre-preprocessing, per
  FR-012a) while `libNSLFrontend.a`'s parser consumes
  *post-preprocessed* text, T2 introduces a thin directive-aware
  pre-pass that splits each input into `(directive token | NSL
  fragment)+`, then feeds each NSL fragment through the existing
  parser to obtain the per-fragment AST/CST. The pre-pass is the
  only new parsing code T2 owns; the inter-directive grammar
  itself is unchanged.
- **FR-019**: The library API MUST be sufficient for T5 to
  implement LSP `textDocument/formatting` and `textDocument/
  rangeFormatting` without modifying T2's source — i.e., it MUST
  accept either a whole-file or line-range request.

#### Test discipline (Principle VI)

- **FR-020**: The T2 deliverable MUST include lit+FileCheck
  fixtures with paired pre-format/post-format golden files for every
  NSL-specific rule in §5.3 (six rules → at least six fixtures), plus
  fixtures for each of the edge cases listed above (parse-error
  refusal, empty input, mixed line endings, etc.).
- **FR-021**: The T2 deliverable MUST include an idempotence
  fixture that runs `nsl-fmt | nsl-fmt` and asserts byte-for-byte
  equality.
- **FR-022**: The T2 deliverable MUST include a CLI-vs-library
  parity GoogleTest that asserts the CLI and library entry points
  produce byte-identical output for the same input (gates FR-017 /
  FR-019).

### Key Entities

- **CST (Concrete Syntax Tree)**: per [`nsl_tooling_design.md`](../../docs/design/nsl_tooling_design.md)
  §2.4 — a parse representation that preserves trivia (whitespace,
  comments, blank lines) between tokens **and preserves
  preprocessor directives as opaque tokens** (per FR-012a). The
  formatter walks the CST; the AST alone is insufficient because
  it discards both trivia and directives. T2 owns the delivery of
  this layer (it is a T2-only consumer in this milestone; T5 will
  later add the LSP "format" path on top). Note: this implies the
  T2 CST is *pre-preprocessor*, parsed from raw source — distinct
  from the existing `libNSLFrontend.a` AST which is built from
  post-preprocessed text.
- **Doc IR (Wadler–Leijen layout commands)**: per §5.2 — the typed
  intermediate (`Text`, `Line`, `Nest`, `Group`, `Concat`, `Align`,
  `Comment`) emitted by the LayoutPlanner and consumed by the
  LayoutRenderer. Internal to `libNslFmt.a`.
- **Configuration record**: in-memory representation of
  `.nsl-fmt.toml`, with the ten keys in FR-014 and the built-in
  defaults from §5.1.
- **Diagnostic**: surface-level error returned to the CLI / library
  caller when input cannot be formatted (parse error, malformed
  config, mutually-exclusive flags). Reuses the existing project
  `DiagnosticEngine` ([`nsl_compiler_design.md`](../../docs/design/nsl_compiler_design.md)
  §12) — does not introduce a parallel error type.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: A new contributor with no prior exposure to the NSL
  style can format their first edited file with one command
  (`nsl-fmt -i foo.nsl`) without reading a style guide; reviewer
  comments about whitespace/indent on their first PR drop to zero.
- **SC-002**: Formatting any of the seven audited corpus files
  (`rv32x_dev`, `turboV`, `mmcspi`, `SDRAM_Controler`,
  `mips32_single_cycle`, `ahb_lite_nsl`, `cpu16` — per
  `docs/CLAUDE.md` §10) twice produces byte-identical output on the
  second run (idempotence holds across the entire audited corpus).
- **SC-003**: `nsl-fmt --check` on a 1000-line NSL file returns in
  under 250 ms on the dev-container hardware target (interactive
  CI feedback).
- **SC-004**: After T2 lands and project CI runs `nsl-fmt --check`
  as a required step, the rate of style-only revert/fixup commits
  on the `master` branch falls to zero within one release cycle.
- **SC-005**: When T5 is later implemented, the LSP's
  `textDocument/formatting` handler is at most ~30 lines of glue
  (forwarding the request to the `libNslFmt.a` entry point and
  packaging the result as `TextEdit[]`); zero formatter logic is
  re-implemented (Principle II, validated by a code-review
  checkpoint at T5 merge).

## Assumptions

### Reasonable defaults adopted (no clarification needed)

- **Diff format for `--check`**: unified diff (`---`/`+++` headers,
  `@@` hunks). This is the universal convention for `gofmt --diff`,
  `clang-format -output-replacements-xml=false`, etc.
- **Config discovery is "first one wins" walking upward** from the
  input file (or CWD with `--stdin`). Matches `clang-format`'s
  `.clang-format` discovery and `rustfmt`'s `rustfmt.toml`
  discovery.
- **Built-in defaults match the example `.nsl-fmt.toml` shown in
  §5.1** (indent = 4, max_line_length = 100, etc.).
- **No `--write-mode=atomic` flag** — `-i` is always atomic
  (write-to-temp + rename); a non-atomic mode is an attractive
  nuisance (corrupts source on crash) with no real-world use case.
- **Numeric literal preservation**: forms (`0xFF`, `0b1010`, `42`,
  `8'b1010`, `Z`/`X`/`U` value literals) are byte-preserved (FR-011);
  the design doc §5.3 lists no toggle for canonicalization, so
  preservation is the conservative default.
- **Trailing newline** at end of file: output ALWAYS ends with
  exactly one `\n` (clarified Session 2026-05-05). If input lacks
  one, formatter ADDS it. If input has multiple trailing blank
  lines, formatter NORMALIZES to a single `\n`. Matches gofmt /
  rustfmt / black convention; idempotent by construction. Aligns
  with the project convention (every file in `lib/` and
  `include/nsl/` has exactly one trailing newline).
- **Tab character handling**: `indent = "tab"` in `.nsl-fmt.toml`
  emits literal `\t` for indentation; mixed tab/space indent in
  input is normalized to the configured indent.
- **Build target**: T2 builds inside the `ghcr.io/koyamanx/
  nsl-nslc:dev` container only (per project memory
  `project_build_environment.md`); host-machine builds are not a
  supported configuration.

### Scope boundaries

- **LSP integration is OUT of scope.** T2 ships `libNslFmt.a` and
  the public header; T5 is the milestone that wires it into
  `nsl-lsp`'s `textDocument/formatting` handler.
- **Pre-commit hook recipe is OUT of scope** (per
  [`CLAUDE.md`](../../CLAUDE.md) §2.3 — pre-commit hook recipe
  lands at T12).
- **`nsl-fmt` does not run as a daemon / server.** Each invocation
  is a one-shot process; LSP-style persistent state lands at T5.
- **Tooling for non-`.nsl` files** (e.g., `.json` config, `.toml`
  itself, `.md`) is OUT of scope — `nsl-fmt` formats NSL source only.
- **`@`-prefixed annotation extensions, vendor pragmas, comments
  with embedded directives** (e.g., `// nsl-fmt: off`/`on`
  islands) are OUT of scope for T2 — can be added as a follow-up
  amendment without affecting T2's wire-level shape.

### Dependencies on existing systems

- **Reuses `libNSLFrontend.a`** (lexer / token tables / parser /
  source-location / diagnostics). Per FR-018, T2 owns one new
  piece of parsing code: a directive-aware pre-pass that splits
  raw input into directive tokens + NSL fragments. NSL fragments
  go through the existing parser (extended only with a
  CST-emitting mode per §2.4 to retain trivia). This is the
  no-duplication rule (Principle II) restated as a build-time
  constraint.
- **Builds inside the published `ghcr.io/koyamanx/nsl-nslc:dev`
  container** (per project memory `project_build_environment.md`);
  no host-machine LLVM is required.
- **CI runs in GitHub Actions** with both clang and gcc cells (per
  the existing M0 reproducibility-CI scaffolding); the lit cell
  runs without ASan (per project memory `libmlir_asan_mismatch`).
- **Audited NSL corpus** (the seven projects vendored under
  `audited/` per Principle V — `P-VEN`) is the ground-truth input
  set for the cross-project idempotence assertion in SC-002.

