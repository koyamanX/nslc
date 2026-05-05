<!-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception -->

# Phase 0 Research: T2 — Formatter v0 (`nsl-fmt`)

**Branch**: `010-t2-formatter-v0` | **Date**: 2026-05-04
**Plan**: [plan.md](./plan.md)
**Spec**: [spec.md](./spec.md)

This document resolves the design unknowns that surfaced during
plan authoring, in the standard Decision / Rationale / Alternatives
shape. Each entry corresponds to one question that *would* have
been a `NEEDS CLARIFICATION` in the plan's Technical Context if
left unanswered. The three load-bearing user-facing decisions
(Q1 / Q2 / Q3) are already locked by `/speckit-clarify`
Session 2026-05-04 and recorded in
[`spec.md`](./spec.md) `## Clarifications`; this file expands them
into the implementation-level choices they imply.

---

## §1. Directive-aware pre-pass — implementation shape

**Decision**: A line-oriented scanner in
`lib/Fmt/DirectiveSplitter.cpp` that (a) consumes raw source
character-by-character, (b) emits a `DirectiveTok` whenever the
first non-whitespace character on a line is `#` (one of the seven
directive opcodes from `pp.ebnf` §2: `#include`, `#define`,
`#undef`, `#ifdef`, `#ifndef`, `#if`, `#else`, `#endif`,
`#line`) — including all continuation lines joined by `\` — and
(c) emits an `NSLFragment` token range otherwise. `%IDENT%`
splices are NOT routed to the directive splitter; they appear
*inside* NSL fragments and are handled at CST level (see §7).

**Rationale**:
- The grammar in `pp.ebnf` §1 is explicit that directives are
  line-oriented (P1: "the preprocessor operates line-by-line").
  A line scanner is the smallest implementation that captures this.
- The directive payload is preserved byte-for-byte (FR-012a — "The
  formatter MUST NOT reorder, deduplicate, or syntactically
  rewrite directives"); a scanner-only approach makes this trivial
  by never tokenizing the payload past the leading opcode.
- Routing NSL fragments through the existing
  `libNSLFrontend.a` parser preserves Principle II's no-
  duplication rule (FR-018) — the only net-new parsing code is
  the splitter.
- Line-continuation handling (`\` at end-of-line, allowed in C-
  style preprocessors and assumed inherited by NSL per `pp.ebnf`
  §2.2) is a small fixed extension; we explicitly handle it to
  avoid a directive being split mid-line.

**Alternatives considered**:
- **Treat directives as opaque tokens *inside* the existing
  preprocessor pipeline**: rejected because the existing
  preprocessor *consumes* directives (per `pp.ebnf` P12 — the
  preprocessor/parser seam) and the formatter needs them
  preserved. Modifying the preprocessor to optionally retain
  directives would entangle the formatter with M1's invariants.
- **A full second NSL parser that grammatically understands
  directives in-place**: rejected as a Principle II violation —
  parallel parsers diverge.
- **Token-stream filter on `Lexer` output**: rejected because
  the lexer also expects post-preprocessed input; we'd be
  re-implementing the directive splitter at the lexer layer.

**Test fixtures gating this decision**:
`test/Fmt/directives/{include-passthrough,ifdef-island,define-passthrough,line-passthrough}/`
plus `test_unit/Fmt/directive_splitter_test.cc` (CST-shape
assertions on synthetic edge cases).

---

## §2. CST-mode parser extension — shape and reuse

**Decision**: Extend `nsl::parse::Parser` with one new boolean
flag `bool emitCST_` (default `false`), initialised at
construction. When set, every `consume()` / `match()` /
`parseProduction()` call additionally appends to a parallel
`CSTBuilder` instance owned by the Parser. The CST is *parallel
to* the AST — same node count, same source ranges, plus
preserved trivia (whitespace + comments) between tokens. The
parser's grammar is unchanged; only the side-effect of emitting
into `CSTBuilder` is gated.

**Rationale**:
- Avoids a parallel parser implementation (Principle II) — every
  production stays in one place.
- The CST-mode flag is a compile-time-trivial extension: ~50 LOC
  in `lib/Parse/CSTMode.cpp`, with the call sites in `Parser.cpp`
  guarded by `if (emitCST_) cstBuilder_->push(...)`. The Parser's
  hot path (sema-only path used by `nslc -emit=hw`) is
  unaffected.
- Pairs with §1: the CSTBuilder receives `NSLFragment` byte
  ranges already split out by the directive pre-pass; the parser
  treats them as ordinary source.
- The CST is a *parser-stage* artifact, distinct from M2/M3's
  AST. Layered-test discipline (Principle VI) supports this:
  parser-stage tests (gtest, AST-snapshot) already exist; CST-
  mode tests will mirror that structure.
- Documentation follows: `docs/design/nsl_tooling_design.md`
  §5.2 architecture diagram is amended to show the CST-mode
  branch off the existing parser (see quickstart §9).

**Alternatives considered**:
- **CST as a post-AST traversal**: rejected because the AST has
  already discarded trivia — we'd have to re-tokenize source to
  recover it.
- **Token-lattice-only approach (no CST tree)**: rejected because
  the layout planner needs structural context (e.g., "this is
  inside an `alt` block") to apply §5.3 rules; a flat token
  list lacks that context.
- **A second top-down parser dedicated to CST**: rejected as a
  Principle II violation.

**Test fixtures gating this decision**:
`test_unit/Fmt/directive_splitter_test.cc` (CST shape) +
the bulk of `test/Fmt/rules/` (which exercises CST-driven layout
on every §5.3 rule).

---

## §3. Wadler–Leijen `Doc` IR — implementation choice

**Decision**: Implement the seven-constructor `Doc` algebra
(`Text`, `Line`, `Nest`, `Group`, `Concat`, `Align`, `Comment`)
in-tree under `lib/Fmt/Doc.{h,cpp}` per the type sketch in
[`docs/design/nsl_tooling_design.md`](../../docs/design/nsl_tooling_design.md)
§5.2. Use `std::variant<Text, Line, ...>` for the discriminated
union (C++17 — Principle "Build Standards"). Storage is a tree
of `std::shared_ptr<Doc>` (the algebra is small and read-only after
construction; a shared-ownership model keeps the planner code
clean and avoids manual lifetime management).

**Rationale**:
- The Wadler–Leijen algebra is ~150 LOC with the renderer; an
  external dependency would add a build-system burden far
  exceeding the implementation cost.
- The seven constructors map 1:1 to the design doc — no
  redesign needed.
- `std::variant` + `std::visit` matches the project's existing
  C++17 idiom (used in `lib/AST/` for expression visitors).
- `std::shared_ptr` is the project's RAII default for tree-
  shaped IR (matches `lib/AST/` ownership conventions).

**Alternatives considered**:
- **Vendor Bernardy's "A Pretty But Not Greedy Printer"** (2017
  Haskell paper, multiple C++ ports): rejected because the
  algorithm is geared at minimum-overflow layout, which is
  *more* than the design doc requests; the simpler Wadler–
  Leijen algebra is sufficient and matches §5.2 verbatim.
- **`llvm::FormatVariadic`**: rejected because it's a printf-
  style formatter, not a layout-deciding pretty-printer.
- **Boost.Spirit's pretty-printer adapter**: rejected because
  Boost is not in the project's dependency graph and adding it
  for one library is a large blast radius.

---

## §4. TOML config-parser library — vendoring choice

**Decision**: Vendor **`toml++` v3.4** (Mark Gillard, MIT
license, single-header) under `third_party/tomlpp/`, with a
`PROVENANCE.md` per Constitution Principle V's vendoring
discipline. Configuration parsing in `lib/Fmt/Config.cpp` calls
`toml::parse_file()` directly; the result is mapped to the
`Configuration` record (data-model §5).

**Rationale**:
- toml++ is single-header (~10 K LOC), MIT-licensed
  (Apache-2.0-compatible per Apache 2.0 §5 "Submission of
  Contributions"), and supports TOML v1.0.0 — the version the
  example `.nsl-fmt.toml` in design doc §5.1 implicitly uses.
- Vendoring (vs git submodule) matches Principle V's
  reproducibility rule: configure-time fetches are prohibited
  for audited dependencies, and a single header is the cleanest
  vendor case.
- The library is mature (5+ years, broad use), header-only
  (no build-system surgery), and supports C++17 (the project's
  language baseline).
- The PROVENANCE.md model is already in use for audited NSL
  projects (Principle VI / V); reusing the pattern is a small
  precedent extension to third-party libs.

**Alternatives considered**:
- **`cpptoml`**: rejected because upstream has been unmaintained
  since 2020; toml++ has active maintenance.
- **`tomlplusplus` shipped via vcpkg / Conan**: rejected — the
  project does not use a package manager (CMake + vendored
  deps; matches LLVM/CIRCT convention).
- **Roll our own minimal TOML subset parser**: rejected because
  the design doc's `.nsl-fmt.toml` uses inline strings, integer
  values, boolean values, and quoted strings — a non-trivial
  subset to write and maintain correctly.
- **JSON instead of TOML**: rejected — design doc §5.1
  explicitly specifies `.nsl-fmt.toml`; changing the format
  would require an amendment.

**License compatibility**: toml++ is MIT-licensed; Apache 2.0 +
LLVM Exception (the project license) is compatible with MIT in
both directions. The `PROVENANCE.md` will record this explicitly
for the eventual `nsl-release` license-audit gate (Constitution
Principle IX § Release artifacts; M9).

---

## §5. Unified-diff emitter — in-tree vs library

**Decision**: Implement a small Myers-diff-based unified-diff
emitter in-tree at `lib/Fmt/Diff.{h,cpp}` (~80 LOC). Output
matches `diff -u` format: `--- old.nsl\n+++ new.nsl\n@@ -1,5 +1,5
@@\n` plus prefixed lines. No external dependency.

**Rationale**:
- A unified-diff emitter is not on the formatter's hot path
  (only `--check` exercises it); algorithmic sophistication
  doesn't matter at the file sizes the formatter sees.
- LLVM ships `llvm::DiffPrinter` but it's an internal API of
  llvm/Support's `tools/llvm-cov` — not a stable public surface.
- A 100-LOC Myers implementation is a well-known algorithm
  (E. W. Myers, 1986) with a stable O(ND) bound that's more
  than fast enough for the SC-003 250 ms / 1000-line budget.
- Avoids pulling `libxdiff` (git's diff library) as a vendored
  dependency for ~80 LOC of work.

**Alternatives considered**:
- **`libxdiff` (vendored)**: rejected — pulling git's diff lib
  for one feature is over-engineered; it would also add a
  PROVENANCE.md and a license-audit row for ~80 LOC of saved
  work.
- **Shell-out to `/usr/bin/diff -u`**: rejected — not portable
  to all dev environments, slow (process-creation cost would
  blow SC-003), and a rough fit for the library API.
- **Defer `--check` to T5**: rejected — `--check` is in scope
  for T2 per CLAUDE.md §2.3 and is essential to FR-003.

---

## §6. Audited-corpus availability — current vs T2

**Decision**: T2 wires the audited-corpus idempotence sweep
(SC-002, FR-021) at `test/Fmt/idempotence/audited/<project>.test`,
each fixture being a `RUN: nsl-fmt --stdin <
%S/../../audited/<project>/<file>.nsl | nsl-fmt --stdin |
diff -q - <(nsl-fmt --stdin < %S/../../audited/<project>/<file>.nsl)`
shape. **However**: per Principle V's `P-VEN` milestone, the
seven audited projects are vendored at M7 (post-T2). At T2
acceptance, the fixtures will reference `test/audited/<project>/`
paths that don't exist yet; the lit fixtures will be
`UNSUPPORTED:` until the project tree appears, then auto-activate
once M7 lands.

**Rationale**:
- The fixture wiring is small and load-bearing for SC-002; it's
  better to land it now (with `UNSUPPORTED:` markers) than to
  retrofit at M7 and risk forgetting.
- `UNSUPPORTED:` is the standard lit mechanism for tests that
  depend on a future milestone — the M5 plan uses the same
  pattern for end-to-end tests.
- The synthetic idempotence corpus
  (`test/Fmt/idempotence/synthetic/`) is independent of the
  audited corpus and runs in CI from T2 day-one — it covers the
  edge cases the audited corpus might miss (empty input, BOM,
  CRLF, parse-error refusal, over-long lines). SC-002 is fully
  enforced once M7 lands without any T2-side changes.
- This is consistent with how M5's M3-corpus extension was
  scaffolded (M5's `test/Lower/m3_corpus/s<NN>/` initially had
  many `UNSUPPORTED:` rows that have since lit up as the
  underlying coverage landed).

**Alternatives considered**:
- **Defer the audited-corpus fixture wiring to M7**: rejected
  because the audited-corpus idempotence is a load-bearing
  Success Criterion (SC-002); deferring the wiring risks the
  test never being written.
- **Vendor the audited projects at T2**: rejected because
  `P-VEN` is its own milestone with its own license-audit / PR
  scope; doing it inside T2 would balloon the change.
- **Use a placeholder corpus** (e.g., compiler self-tests):
  rejected — not the same as the seven Principle-VI-named
  projects; would not satisfy SC-002.

**Status note**: At T2 acceptance, the audited-corpus fixtures
are present but skipped (`UNSUPPORTED:`); the synthetic corpus
is fully active. The CI step `nsl-fmt --check
test/audited/**/*.nsl` is a `|| true`-guarded shell glob at T2
that becomes a hard gate at M7 (one-line ci.sh edit).

---

## §7. `%IDENT%` macro splice handling

**Decision**: `%IDENT%` splices are NOT routed to the directive
pre-pass (§1); they appear *inside* NSL fragments and are
handled at the lexer + CST level. The lexer recognises
`%[A-Za-z_][A-Za-z0-9_]*%` as a single token kind
(`tok::IdentSplice`); the CSTBuilder records the literal token
text. The LayoutPlanner treats `IdentSplice` as a `Doc::Text`
leaf — opaque, byte-preserved, never reformatted.

**Rationale**:
- `%IDENT%` is a *post-preprocessor* artifact (P3) — it's the
  textual residue of macro splicing and may appear anywhere a
  bare identifier may. Recognising it at lex level matches the
  M1 lexer's existing token table.
- Treating it as `Doc::Text` (atomic, never broken across
  lines) preserves the splice's identity and avoids the
  formatter accidentally rewriting `%FOO%` to `%foo %` or
  similar.
- M5's `NSLCheckSemanticsPass` expects `%IDENT%` residue to be
  ZERO at post-pipeline (per FR-012a-equivalent for the
  compiler); the formatter is upstream of the compiler and
  *preserves* `%IDENT%` (which is then consumed by the
  preprocessor when the compiler runs). No conflict.

**Alternatives considered**:
- **Route `%IDENT%` through the directive pre-pass**: rejected
  — `%IDENT%` is a token, not a line directive; mixing them
  in the splitter would tangle the abstraction.
- **Force `%IDENT%` through the existing macro expander before
  formatting**: rejected — that would lose the splice in the
  formatted output, breaking byte-for-byte directive
  preservation (FR-012a).

---

## §8. CLI argv parser — `cl::opt` vs custom

**Decision**: Use **LLVM's `cl::opt`** in `tools/nsl-fmt/main.cpp`,
matching the existing project tools (`nslc`, `nsl-opt`). One
`cl::opt<bool>` per flag (`-i`, `-c`, `--stdin`, `--config`,
`--range`), one `cl::list<std::string>` for positional file
arguments. Mutually-exclusive validation (FR-006) happens after
`cl::ParseCommandLineOptions()` returns.

**Rationale**:
- `cl::opt` is already in the link graph (every existing tool
  uses it); zero new dependency cost.
- Auto-generated `--help` output matches LLVM/CIRCT convention
  — important for cross-tool consistency.
- LLVM's option parser handles `--flag=value`, `--flag value`,
  `-flag`, `-flag=value` uniformly — matches the design doc's
  CLI sketch.
- The mutually-exclusive check (FR-006) is small and explicit —
  no need for a more sophisticated argv framework.

**Alternatives considered**:
- **Vendor `CLI11`**: rejected — extra dependency for no
  marginal feature; project standard is `cl::opt`.
- **Custom `argv` walker**: rejected — re-implements something
  every existing tool already has.

---

## §9. Integration with `DiagnosticEngine` (Principle IV)

**Decision**: Every diagnostic emitted by `libNslFmt.a` (parse
error from the underlying `nsl-parse` invocation, config-file
malformed error, range-out-of-bounds error, unknown TOML key
warning, mutually-exclusive-flags CLI error) flows through
`basic::DiagnosticEngine` — the same sink M2/M3/M5 use. The
`Format::Result` return type carries a
`std::vector<basic::Diagnostic>` for diagnostics emitted during
formatting; the CLI (`tools/nsl-fmt/main.cpp`) prints them on
stderr via the engine's existing renderer.

**Rationale**:
- Principle IV's "every diagnostic flows through the project
  `DiagnosticEngine`" rule applies tool-wide; reusing the
  existing engine guarantees `file:line:col` precision and
  human-vs-JSON renderer parity for free.
- Principle II's no-duplication rule again — the engine is in
  `nsl-basic`; building a parallel one in `lib/Fmt/` would be a
  layering violation.
- The design doc's §5.2 architecture diagram doesn't depict
  diagnostic plumbing; this research note is the design
  decision that gets folded back via the §5.2 amendment
  (quickstart §9).

**Alternatives considered**:
- **A formatter-specific `FmtDiagnostic` type**: rejected —
  Principle II.
- **Print diagnostics directly to stderr without an engine**:
  rejected — loses LSP-renderer compatibility (T5 will need
  JSON-output for `publishDiagnostics`).
- **Suppress all internal diagnostics, return only a bool**:
  rejected — useless for FR-003 ("printing a unified diff per
  offending file") and FR-016 ("emit a stderr error").

---

## §10. Per-fragment parse strategy (Session 2026-05-05 — Q1 strict refusal)

**Decision**: Each `NSLFragment` slice produced by the
DirectiveSplitter (§1) is parsed as a **standalone CompilationUnit**
through a fresh `nsl::SourceManager` + `nsl::Lexer` +
`nsl::parse::parseCompilationUnit()` invocation, with the fragment's
bytes copied into a private in-memory buffer. If `parseCompilationUnit`
returns `nullptr` OR the per-fragment `DiagnosticEngine.hasError()`
is true, `format_buffer` returns `Status::Refused` and copies the
fragment's diagnostics into `FormatResult::diagnostics`. ALL other
fragments and directives are NOT processed further (the file is
refused atomically — no partial output).

**Rationale**:
- Spec FR-012 (clarified Session 2026-05-05) mandates strict refusal:
  any input the lex+parse pipeline rejects → exit non-zero. Per-
  fragment parsing makes this granular: a single bad fragment
  refuses the whole file.
- The "fresh SourceManager per fragment" model avoids polluting
  the caller's SourceManager with the formatter's transient state.
  It does mean diagnostic SourceLocations reference the PRIVATE
  per-fragment FileID — the CLI renderer translates to "byte
  offset N within fragment K" rather than "file:line:col" against
  the original input. Full source-mapping is a Phase 3+ refinement.
- Fragments that AREN'T well-formed top-level NSL (e.g., the body
  of a `module` block split mid-way by `#ifdef`/`#endif`) WILL
  fail to parse as a standalone CompilationUnit. This is the
  expected, acceptable cost of the directive-aware strategy:
  `clang-format`'s preprocessor model has the same limitation —
  some valid C/C++ source with directive-split bodies cannot be
  formatted by clang-format and must be reformatted manually.

**Alternatives considered**:
- **Wrap each fragment in a synthetic `module __wrapper__ { ... }`
  before parsing**: rejected — would silently re-classify
  top-level declarations as internal-structure declarations
  (RegDecl inside a module body is parsed differently from a
  `param_int` at top level), breaking the layout rules.
- **Add a "lex this byte range only" mode to the M1 lexer**:
  rejected for T2 — would require modifying the M2-frozen Lexer
  with a sub-buffer construction path. Defer to a future M-track
  amendment if directive-split fragments become a common pain
  point.
- **Parse the entire raw source as one CompilationUnit (ignoring
  directives)**: rejected — directive lines aren't valid NSL
  tokens; the parser would error immediately on the first `#`.
  This was the Phase-3c attempt that broke the BOM/`%IDENT%`/over-
  long-line fixtures.

**Test fixtures gating this decision**:
- T039 (parse-error refusal) lit fixture exercises the refusal
  path with a single-fragment input.
- A new fixture under `test/Fmt/edge/parse-error-refusal/`
  exercises the "one good fragment + one bad fragment → atomic
  refusal" case (added when T059 lands).

**Impact on existing fixtures (T2 milestone follow-up)**:
- `test/Fmt/edge/bom-preserve/utf8-bom.test` — DELETE (BOM input
  is refused per Q1; the fixture's premise is invalidated).
- `test/Fmt/edge/over-long-line/string-no-break-point.test` —
  REWRITE the input as `module m { func f() { _display("xxx…"); } }`
  so the `_display(...);` lives inside a `func` body (legal NSL).
  The over-long-line preservation still gets tested.

## §11. Inline-comment preservation (Session 2026-05-05 — Q2)

**Decision**: Comments BETWEEN tokens of a single statement (e.g.,
`reg /* width 8 */ q[8];`) are recorded by the CSTBuilder as a
new `Trivia.kind = InlineBlock` (or `InlineLine`) variant attached
to the FOLLOWING token's `leadingTrivia`. The LayoutPlanner emits
the comment between the same two tokens in the canonical output;
the spaces before + after the comment are normalized to one space
each (unless `preserve_comments != All`).

**Rationale**:
- FR-010 (clarified Session 2026-05-05) requires preservation of
  inline comments at the same token-relative position.
- Hoisting comments to leading or trailing line position would
  lose semantic information (e.g., `wire a + /* trace */ b` —
  the `/* trace */` refers to the `+` operation, not the
  surrounding declaration).
- Idempotence (FR-008) is preserved by construction: the
  canonical form has exactly one space on each side of the
  comment, so re-parsing → re-emitting yields the same bytes.

**Alternatives considered**:
- **Hoist to leading position**: rejected (loses semantic
  position; can mislead the reader).
- **Refuse to format any declaration containing inline
  comments**: rejected (breaks reformatting on real code that
  uses inline annotations).

## §12. Trailing-newline normalization (Session 2026-05-05 — Q3)

**Decision**: `format_buffer`'s `Status::Success` output ALWAYS
ends with exactly one `\n`. When non-empty, the LayoutRenderer's
final emission step appends `\n` if the last byte is not already
`\n`; sequences of trailing blank lines (multiple consecutive
`\n` at the end) are collapsed to one. Empty input → empty output
(no spurious `\n`).

**Rationale**:
- FR-008 idempotence is preserved by construction: canonical form
  has exactly one trailing `\n`, and re-running on it produces
  the same byte sequence.
- Matches gofmt / rustfmt / black convention.
- Aligns with project-wide convention (every file in `lib/` and
  `include/nsl/` has exactly one trailing newline).

**Alternatives considered**:
- **Preserve as-is**: rejected — creates two equivalence classes
  of canonical output (with/without trailing `\n`), conceptually
  noisier and slightly harder to gate in CI (`--check` would have
  to compare byte-for-byte rather than semantically).
- **Refuse if missing**: rejected — POSIX-strict but unfriendly,
  especially for files generated by tools that don't add trailing
  newlines.

## Cross-references

- `Q1` / `Q2` / `Q3`: see [`spec.md`](./spec.md) `## Clarifications`.
- §1 ↔ FR-012a / FR-018; §2 ↔ FR-018; §3 ↔ FR-009 / FR-010;
  §4 ↔ FR-014 / FR-015 / FR-016; §5 ↔ FR-003 / FR-003a;
  §6 ↔ SC-002; §7 ↔ FR-011 (literal preservation extends to
  `%IDENT%` splices); §8 ↔ FR-001..FR-007; §9 ↔ FR-016 / FR-018.
- The Constitution principles each decision touches: §1/§2 →
  Principle II; §4/§5/§6 → Principle V (vendoring +
  determinism); §3/§7/§8/§9 → Principle II (single sourcing of
  primitives).

**Output gate**: ✅ All NEEDS CLARIFICATION resolved; ready for
Phase 1 (data model + contracts + quickstart).
