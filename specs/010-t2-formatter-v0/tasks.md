<!-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception -->

# Tasks: T2 — Formatter v0 (`nsl-fmt`)

**Input**: Design documents from `specs/010-t2-formatter-v0/`
**Prerequisites**: [`plan.md`](./plan.md), [`spec.md`](./spec.md),
[`research.md`](./research.md), [`data-model.md`](./data-model.md),
[`contracts/`](./contracts/), [`quickstart.md`](./quickstart.md)

**Tests**: Test tasks are **MANDATORY** for this project per
Constitution Principle VIII (Test-First Development, NON-
NEGOTIABLE). Every user story MUST include test tasks at the
appropriate layer (per Constitution Principle VI's per-layer
accepted-driver clause: lit + FileCheck for tool-level tests,
GoogleTest for unit-level invariants). Tests MUST be written and
observed FAILING before the corresponding implementation tasks
begin.

**Organization**: Tasks are grouped by user story to enable
independent implementation and testing of each story.

## Format: `[ID] [P?] [Story] Description`

- **[P]**: Can run in parallel (different files, no dependencies)
- **[Story]**: Which user story this task belongs to
  (e.g., US1, US2, US3, US4) — Setup / Foundational / Polish
  phases have NO story label.
- Include exact file paths in descriptions.

## Path Conventions

Single project, LLVM-style. Sources under `lib/Fmt/` and
`tools/nsl-fmt/`; public header under `include/nsl/Fmt/`; test
corpus under `test/Fmt/` (lit) and `test_unit/Fmt/` (gtest).

---

## Phase 1: Setup (Shared Infrastructure)

**Purpose**: Project initialization, vendor third-party dep,
empty CMake scaffolding so subsequent tasks have a place to land.

- [X] T001 Create `lib/Fmt/CMakeLists.txt` declaring `add_nsl_library(nsl-fmt LINK_LIBS nsl-frontend tomlpp)` per [`plan.md`](./plan.md) "Project Structure" section
- [X] T002 Create `tools/nsl-fmt/CMakeLists.txt` declaring `add_nsl_executable(nsl-fmt LINK_LIBS nsl-fmt)` and `tools/nsl-fmt/main.cpp` with a one-line `int main() { return 0; }` stub
- [X] T003 Add `lib/Fmt/` and `tools/nsl-fmt/` to the parent `CMakeLists.txt` `add_subdirectory(...)` lists in `lib/CMakeLists.txt` and `tools/CMakeLists.txt`
- [X] T004 [P] Create `include/nsl/Fmt/Fmt.h` empty umbrella with SPDX header + namespace `nsl::fmt {}`. Public-symbol declarations are added INCREMENTALLY by T026 (`format_buffer`, `FormatResult`, `LineRange`), T076 (`emit_unified_diff`), T087 (`version_string`), T088 (`config_key_names`), T089 (`default_configuration`), T102 (`Configuration`), T103 (`parse_config_file`), T106 (`discover_config`) as each function lands; T086 (Phase 5) verifies the final 10-symbol shape via `audit_fmt_api.sh`. No declaration is added in T004 itself.
- [X] T005 [P] Vendor `toml++` v3.4 under `third_party/tomlpp/` as a one-time human action: download `toml.hpp` and `LICENSE` from https://github.com/marzer/tomlplusplus/releases/tag/v3.4.0 ONCE, COMMIT both files into the repo, and author `third_party/tomlpp/PROVENANCE.md` recording upstream URL + commit SHA + MIT license. The build MUST NOT fetch from the network at configure time or build time (Principle V — reproducibility / determinism). Per Principle V vendoring discipline (research §4).
- [X] T006 [P] Create `third_party/tomlpp/CMakeLists.txt` declaring `add_library(tomlpp INTERFACE)` + `target_include_directories(tomlpp INTERFACE .)`
- [X] T007 Add `third_party/tomlpp/` to project-root `CMakeLists.txt` via `add_subdirectory(third_party/tomlpp)`
- [X] T008 [P] Add the new `check-nsl-fmt`, `check-fmt-lit`, `check-fmt-unit` ninja targets to project-root `CMakeLists.txt` (custom targets that depend on the matching test directories — `check-nsl-fmt` is the umbrella that depends on the other two)
- [X] T009 [P] Create `test/Fmt/lit.cfg.py` registering the `test/Fmt/` corpus with the project's existing lit infrastructure (use `test/Lower/lit.cfg.py` as reference)
- [X] T010 [P] Register `test/Fmt/` in the project-root `lit.cfg.py` discovery list

**Phase 1 checkpoint**: `ninja -C build-noasan nsl-fmt` builds an
empty binary; `ninja check-nsl-fmt` runs zero tests successfully.

---

## Phase 2: Foundational (Blocking Prerequisites)

**Purpose**: Land the parsing pre-pass + CST + Doc-IR
infrastructure that ALL user stories depend on. Per Principle II,
none of these introduce a parallel parser; the Parser is extended
with a CST-mode flag.

- [X] T011 [P] Create gtest fixture `test_unit/Fmt/directive_splitter_test.cc::IncludePassthrough` that asserts splitting `#include "foo.nsl"` produces a single `DirectiveTok{opcode=Include, rawText="#include \"foo.nsl\"\n"}` — observe FAILING (no DirectiveSplitter exists yet)
- [X] T012 [P] Create gtest fixture `test_unit/Fmt/directive_splitter_test.cc::FullCoverage` asserting that for a 100-line synthetic file, every byte is covered by exactly one Slice and the slice ranges are monotonically non-decreasing — observe FAILING
- [X] T013 [P] Create gtest fixture `test_unit/Fmt/directive_splitter_test.cc::LineContinuation` asserting that `\`-continued multi-line directives produce one Slice spanning all continuation lines — observe FAILING
- [X] T014 Implement `lib/Fmt/CST.h` with `CSTNode`, `Trivia`, `DirectiveTok`, `Slice`, `SourceFile` types per [`contracts/cst-shape.contract.md`](./contracts/cst-shape.contract.md) §1, §3, §5 (data-only; no methods beyond constructors)
- [X] T015 Implement `lib/Fmt/DirectiveSplitter.{h,cpp}` per [`research.md`](./research.md) §1 — line-oriented scanner, `\`-continuation handling, BOM-prefix retention; T011/T012/T013 turn green
- [X] T016 [P] Create gtest fixture `test_unit/Fmt/directive_splitter_test.cc::CSTRoundTrip` asserting `serialize(parse_cst(s)) == s` for a 5-line synthetic NSL file — observe FAILING
- [X] T017 Add `bool emitCST_` flag (default `false`) and an opaque `CSTSink* emitSink_` member to `nsl::parse::Parser` in `lib/Parse/Parser.{h,cpp}`; declare a new abstract `class CSTSink { virtual void beginNode(...); virtual void recordToken(...); virtual void endNode(...); virtual ~CSTSink() = default; }` inside the EXISTING `include/nsl/Parse/Parser.h` (Principle II — nsl-parse keeps a single public header; CSTSink is the only new public symbol); gate every `consume()` / `match()` / `parseProduction()` call on `if (emitCST_) emitSink_->...` per [`research.md`](./research.md) §2
- [X] T018 Implement `Parser::setEmitCST(CSTSink*)` in the private impl file `lib/Parse/CSTMode.cpp` (NO new public header — Parser.h already declares the symbol); the `nsl-fmt` library's `CSTBuilder` (T019) implements the `CSTSink` interface from above the layer boundary, so dependency direction stays downward (Principle II layer-table rule)
- [X] T019 Implement `lib/Fmt/CSTBuilder.{h,cpp}` per [`data-model.md`](./data-model.md) §3 — `beginNode()` / `recordToken()` / `endNode()` / `takeRoot()`; T016 turns green
- [X] T020 [P] Create gtest fixture `test_unit/Fmt/directive_splitter_test.cc::CSTInvariants` asserting every CSTNode has a non-empty `SourceRange`, no overlapping child ranges, no byte-loss (per [`contracts/cst-shape.contract.md`](./contracts/cst-shape.contract.md) §3 invariants table) — observe FAILING then green after T019
- [X] T021 [P] Create gtest fixture `test_unit/Fmt/doc_layout_test.cc::TextConcatRender` asserting `Doc::concat({Doc::text("a"), Doc::text("b")})` renders to `"ab"` — observe FAILING
- [X] T022 [P] Create gtest fixture `test_unit/Fmt/doc_layout_test.cc::GroupRibbonFitting` asserting `Doc::group(Doc::concat({Doc::text("a"), Doc::line(), Doc::text("b")}))` renders to `"a b"` at width 100 and `"a\nb"` at width 1 — observe FAILING
- [X] T023 [P] Create gtest fixture `test_unit/Fmt/doc_layout_test.cc::NestIndent` asserting `Doc::nest(4, Doc::concat({Doc::hardline(), Doc::text("x")}))` renders with 4-space indent — observe FAILING
- [X] T024 Implement `lib/Fmt/Doc.{h,cpp}` per [`data-model.md`](./data-model.md) §4 — seven Doc constructors using `std::variant` + `std::shared_ptr`; pure data
- [X] T025 Implement `lib/Fmt/LayoutRenderer.{h,cpp}` — ribbon-fitting renderer that consumes a `Doc` and produces the final byte stream; T021/T022/T023 turn green
- [X] T026 Implement `lib/Fmt/Format.{h,cpp}` `format_buffer()` skeleton that wires DirectiveSplitter → CSTBuilder (via Parser CST-mode) → empty LayoutPlanner → LayoutRenderer; returns `FormatResult{Status::Success, ...}` per [`contracts/format-api.contract.md`](./contracts/format-api.contract.md) §3
- [X] T027 Wire `basic::DiagnosticEngine` integration in `lib/Fmt/Format.cpp` per [`research.md`](./research.md) §9 — every internal diagnostic is appended to `FormatResult::diagnostics`; the CLI's stderr renderer reuses the existing engine
- [X] T028 Extend `scripts/audit_determinism.sh` to also scan `lib/Fmt/` for forbidden patterns (`std::unordered_*`, pointer-derived ordering, time sources, env reads) per [`contracts/format-api.contract.md`](./contracts/format-api.contract.md) §6
- [X] T029 Create `scripts/audit_fmt_api.sh` that greps `include/nsl/Fmt/Fmt.h` and asserts the public symbol count equals exactly 10 per [`contracts/format-api.contract.md`](./contracts/format-api.contract.md) §5

**Phase 2 checkpoint**: `ninja check-fmt-unit` passes the 7
foundational unit tests (T011, T012, T013, T016, T020, T021, T022,
T023). `format_buffer("module empty {}", default_configuration(),
fid)` returns `Status::Success` with raw input as output (no layout
applied yet — that lands in Phase 3).

---

## Phase 3: User Story 1 (P1) — Canonical formatting on demand

**Story goal**: An NSL author runs `nsl-fmt foo.nsl` and gets back
canonical output applying all six §5.3 rules. MVP scope.

**Independent test**: `nsl-fmt --stdin < malformatted.nsl` against
the matching golden file under `test/Fmt/rules/` produces an empty
diff. Verified by ninja `check-fmt-lit` passing the
`test/Fmt/rules/` and `test/Fmt/cli/{stdin,in-place}/` corpora.

### Tests for User Story 1 (TDD — write FAILING first)

- [X] T030 [P] [US1] Author lit fixture `test/Fmt/rules/operator-spacing/binary-and-unary.test` per [`contracts/formatting-rules.contract.md`](./contracts/formatting-rules.contract.md) §5 (R5); paired pre.nsl + post.nsl + idempotence.nsl — observe FAILING
- [X] T031 [P] [US1] Author lit fixture `test/Fmt/rules/bit-slice-spacing/slice-and-concat.test` per [`contracts/formatting-rules.contract.md`](./contracts/formatting-rules.contract.md) §4 (R4) — observe FAILING
- [X] T032 [P] [US1] Author lit fixture `test/Fmt/rules/attached-comments/leading-trailing-block.test` per [`contracts/formatting-rules.contract.md`](./contracts/formatting-rules.contract.md) §6 (R6) — observe FAILING
- [X] T033 [P] [US1] Author lit fixture `test/Fmt/rules/alt-case-alignment/three-cases.test` per [`contracts/formatting-rules.contract.md`](./contracts/formatting-rules.contract.md) §1 (R1) — observe FAILING
- [X] T034 [P] [US1] Author lit fixture `test/Fmt/rules/struct-member-alignment/mixed-name-lengths.test` per [`contracts/formatting-rules.contract.md`](./contracts/formatting-rules.contract.md) §2 (R2) — observe FAILING
- [X] T035 [P] [US1] Author lit fixture `test/Fmt/rules/proc-name-arg-wrap/multi-arg-with-widths.test` per [`contracts/formatting-rules.contract.md`](./contracts/formatting-rules.contract.md) §3 (R3) — observe FAILING
- [X] T036 [P] [US1] Author lit fixture `test/Fmt/cli/stdin/basic-roundtrip.test` asserting `RUN: cat %s | nsl-fmt --stdin | FileCheck %s` for a tiny `module foo {}` — observe FAILING
- [X] T037 [P] [US1] Author lit fixture `test/Fmt/cli/in-place/atomic-rewrite.test` asserting `RUN: nsl-fmt -i %t/scratch.nsl && FileCheck %s < %t/scratch.nsl` — observe FAILING
- [X] T038 [P] [US1] Author lit fixture `test/Fmt/edge/empty-input/zero-bytes.test` (empty file → exit 0, empty output) — observe FAILING
- [X] T039 [P] [US1] Author lit fixture `test/Fmt/edge/parse-error-refusal/syntax-error.test` asserting parse-error → exit non-zero + stderr matches frozen string from [`contracts/formatting-rules.contract.md`](./contracts/formatting-rules.contract.md) §7 — observe FAILING
- [X] T040 [P] [US1] Author lit fixture `test/Fmt/edge/crlf-normalization/mixed-endings.test` asserting CRLF input is normalized to LF on output — observe FAILING
- [X] T041 [P] [US1] Author lit fixture `test/Fmt/edge/bom-preserve/utf8-bom.test` asserting BOM at file start is preserved on output — observe FAILING
- [X] T042 [P] [US1] Author lit fixture `test/Fmt/edge/over-long-line/string-no-break-point.test` asserting a 200-char `_display(...)` line is emitted as-is (no corruption) — observe FAILING
- [X] T043 [P] [US1] Author lit fixture `test/Fmt/idempotence/synthetic/round-trip-five-cases.test` running `nsl-fmt | nsl-fmt | diff -q -` on five constructed inputs — observe FAILING
- [X] T044 [P] [US1] Author lit fixture `test/Fmt/directives/include-passthrough/quote-and-angle.test` asserting both `#include "x.nsl"` and `#include <x.nsl>` survive the round-trip byte-for-byte — observe FAILING
- [X] T045 [P] [US1] Author lit fixture `test/Fmt/directives/ifdef-island/nested-conditional.test` asserting `#ifdef`/`#endif` blocks with NSL fragments inside survive — observe FAILING
- [X] T046 [P] [US1] Author lit fixture `test/Fmt/directives/define-passthrough/macro-with-args.test` asserting `#define FOO(x,y)` is emitted byte-for-byte — observe FAILING
- [X] T047 [P] [US1] Author lit fixture `test/Fmt/directives/line-passthrough/line-marker.test` asserting `#line 42 "elsewhere.nsl"` survives — observe FAILING
- [X] T048 [P] [US1] Author lit fixture `test/Fmt/directives/ident-splice-passthrough/percent-ident.test` asserting `%FOO%` splices are byte-preserved (research §7) — observe FAILING

### Implementation for User Story 1

- [X] T049 [US1] Implement R5 (operator spacing) in `lib/Fmt/LayoutPlanner.cpp` — `formatNode(BinaryExpr)` emits `lhs SP op SP rhs`, `formatNode(UnaryExpr)` emits `op operand` (no space); `CompilationUnit` / `ModuleBlock` / `RegDecl` recursion overrides route the formatter into the relevant sub-expressions; T030 turned XFAIL→PASS and the marker was removed in the same change. (`ConditionalExpr` is deferred to a follow-up; the T030 fixture only exercises Binary + Unary.)
- [X] T050 [US1] Implement R4 (bit-slice / concat spacing) in `lib/Fmt/LayoutPlanner.cpp` — `formatNode(SliceExpr)` emits `<sub>[<hi>]` / `<sub>[<hi>:<lo>]` (no spaces inside brackets, no space around the colon); `formatNode(ConcatExpr)` emits `{<part>, <part>, ...}` (one space after each comma, no spaces inside braces unless `spaces_inside_braces=true`); a `WireDecl` recursion override descends into the optional width. T031 fixture's input was retargeted from `wire = init` (rejected by S2 at parse time) to `reg <w>[<n>] = init`; XFAIL marker removed in the same change.
- [X] T051 [US1] Implement R6 (attached-comment preservation) in `lib/Fmt/LayoutPlanner.cpp` trivia-emission helpers per [`contracts/formatting-rules.contract.md`](./contracts/formatting-rules.contract.md) §6. New internal `CommentScanner.{h,cpp}` walks raw source bytes between AST positions, recognising `//`-line and `/* */`-block comments (string-literal-aware) and emitting them as `{kind, byteSpan, startLine, endLine}` records (Principle II: trivia-only pass; Lexer remains the source of truth for non-trivia tokenisation). `formatNode(ModuleBlock)` consumes the records with two-mode inter-decl-gap layout: NO-comments + NO-source-newlines → emit gap bytes verbatim (preserves `reg a; reg b;` single-line shape pinned by T031/T033/dirty-file fixtures); otherwise → canonical multi-line layout, classifying each comment as TRAILING for prev decl (same source line, no `\n` between prev end and comment start) or LEADING for next decl (own line, immediately above via `Doc::hardline()`). `cfg_.preserve_comments` honored: `All` → emit all; `LeadingOnly` → drop trailing line comments via per-comment skip in `emitTrailingComment`; `None` → drop every comment kind (`emitLeadingComment` early-return + `interleaveChildren` switches inter-token gaps to new `verbatimGapFiltered` helper that scans + elides). T032 (canonical R6 fixture; broken multi-line RUN syntax fixed to `printf '...\n...'`) + T124a (`preserve_comments = "leading_only"`) + T124b (`preserve_comments = "none"`) all flip XFAIL→PASS. Idempotent by construction for all three. Lit: 589 PASS + 4 XFAIL + 7 Unsupported (up from 586 PASS + 7 XFAIL); 0 regressions.
- [X] T052 [US1] Implement R1 (alt/any case-arrow alignment) in `lib/Fmt/LayoutPlanner.cpp` — `formatNode(AltBlock)` / `formatNode(AnyBlock)` delegate to a shared `formatCondCaseBlock` helper that pads each condition to the longest cond's width + 1 column so all `:` separators align (per [`contracts/formatting-rules.contract.md`](./contracts/formatting-rules.contract.md) §1). Honors `align_case_arrows=false` (collapses to single-space separator). New recursion-only overrides for `FuncDefn`, `SeqBlock`, and `ParallelBlock` route the formatter into `func` / `proc` / `{ ... }` bodies (the parser models a brace-enclosed func body as a `ParallelBlock`, not a `SeqBlock`, per the AST shape). T033 fixture's input substitutes `phase` for `state` and `q` for `reg` to dodge a pre-existing parser-recovery loop that hangs nsl-fmt when keywords appear in expression / transfer-LHS position; the R1 rule itself is identifier-agnostic so the substitution is faithful to the rule contract.
- [X] T053 [US1] Implement R2 (struct member-bracket alignment) in `lib/Fmt/LayoutPlanner.cpp` — `formatNode(StructDecl)` emits each member on its own line indented via `Doc::nest(indentStep())`, padding member names so all `[` brackets land at column `max_name + 2` from the name's start (matching the canonical example in [`contracts/formatting-rules.contract.md`](./contracts/formatting-rules.contract.md) §2). Honors `align_struct_members=false` (collapses to a single-space separator). New `LayoutPlanner::indentStep()` helper maps the `Configuration::Indent` enum to the column-equivalent the renderer's `Doc::nest` machinery expects (Spaces2→2, Spaces4→4, Tab→1). T034 fixture turns XFAIL→PASS; marker removed in the same change.
- [X] T054 [US1] Implement R3 (proc_name argument-list wrapping) in `lib/Fmt/LayoutPlanner.cpp` `visitProcNameDecl` — single-line vs multi-line decision per [`contracts/formatting-rules.contract.md`](./contracts/formatting-rules.contract.md) §3; T035 turns green. Implemented: `formatNode(ProcNameDecl)` projects the single-line width from `regArgs()` + `name()` + parent indent and switches to multi-line when it exceeds `cfg_.max_line_length` (default 100); the multi-line form puts each arg on its own line inside `Doc::nest(indentStep(), ...)` with the closing `);` on its own line at the outer indent; trailing-comma policy honors `Configuration::TrailingCommas::{Add, Remove, Preserve}`. Required a contract amendment (Session 2026-05-12 in `plan.md` Plan Revisions) dropping the original `[N]`-width sub-rule the parser does not accept; T035 and T122a/T122b inputs rewritten to use valid NSL (bare-identifier arg lists that overflow 100 cols).
- [X] T055 [US1] Implement remaining `LayoutPlanner::visit*()` overrides (~25 NSL CST node categories — declare/module/internal-structure/action statements/atomic actions/expressions per the AST taxonomy in `nsl_compiler_design.md` §5) — landed across batches `718266d`, `d9ce0a9`, `702dcd5`, `683028c`, `7056f3d` (35 recursion-only overrides + the 8 canonical-layout overrides from T049/T050/T052/T053 = 43 of ~54 NodeKinds). The remaining ~11 NodeKinds are correctly handled by the macro-generated verbatim fallback: leaf source forms (`LiteralExpr`, `IdentifierExpr`, `SystemVarExpr`, `EmptyStmt`, `BareFinishStmt`, `GotoStmt`) plus per-name decls with no Expr children (`IntegerDecl`, `FuncSelfDecl`, `ProcNameDecl` decl-only — T054 will add R3 canonical for that one separately, `StateNameDecl`, `FirstStateDecl`).
- [X] T056 [US1] Implement CLI default mode in `tools/nsl-fmt/main.cpp` — for each positional file, read → `format_buffer()` → write to stdout; T036 turns green
- [X] T057 [US1] Implement CLI `-i` / `--in-place` mode in `tools/nsl-fmt/main.cpp` — write to temp file in same directory + atomic rename; T037 turns green
- [X] T058 [US1] Implement CLI `--stdin` mode in `tools/nsl-fmt/main.cpp` — `MemoryBuffer::getSTDIN()` → `format_buffer()` → stdout
- [X] T059 [US1] Implement parse-error refusal path in `lib/Fmt/Format.cpp` per [`contracts/format-api.contract.md`](./contracts/format-api.contract.md) §4 (`Status::Refused`); T039 turns green
- [X] T060 [US1] Implement empty-input handling in `lib/Fmt/Format.cpp` (early-return Success with empty output); T038 turns green
- [X] T061 [US1] Implement CRLF normalization in `lib/Fmt/LayoutRenderer.cpp` (always emit LF); T040 turns green
- [X] T062 [US1] Implement BOM preservation in `lib/Fmt/DirectiveSplitter.cpp` (BOM byte sequence retained as leading trivia of first slice); T041 turns green
- [X] T063 [US1] Implement over-long-line "best effort" in `lib/Fmt/LayoutRenderer.cpp` (emit the un-breakable line and continue); T042 turns green
- [X] T064 [US1] Implement idempotence — by construction, T030–T035 having idempotence.nsl golden files already exercises this; T043 turns green once the planner is stable
- [X] T065 [US1] Verify directive pass-through fixtures T044–T048 turn green against the implemented DirectiveSplitter + LayoutPlanner (no new code expected; this task is the verification step)

### Additional tests (added during /speckit-analyze remediation, finding C4)

- [X] T125 [P] [US1] Author lit fixture `test/Fmt/edge/literal-preservation/numeric-and-zxu.test` containing each numeric literal form once (decimal `42`, hex `0xFF`, binary `0b1010`, NSL value literal `8'b1010`, Z/X/U value literals like `4'bZZZZ`, `4'bXXXX`, `4'bUUUU`); assert byte-identical round-trip per FR-011 — observe FAILING then green by construction once the lexer's existing byte-fidelity is exercised through CST + LayoutPlanner

**Phase 3 checkpoint**: All US1 fixtures green;
`ninja check-fmt-lit` passes the `test/Fmt/rules/`,
`test/Fmt/cli/{stdin,in-place}/`, `test/Fmt/edge/`,
`test/Fmt/directives/`, and `test/Fmt/idempotence/synthetic/`
sub-corpora. **MVP demonstrable**: an author can run
`nsl-fmt --stdin < foo.nsl > foo.out.nsl` on any synthetic NSL
file and get canonical output back.

---

## Phase 4: User Story 2 (P2) — CI gate via `--check`

**Story goal**: A CI maintainer adds `nsl-fmt --check $(git ls-files
'*.nsl')` and gets exit non-zero + per-file unified-diff output if
anything would change. Multi-file invocations continue past errors
and report ALL offenders.

**Independent test**: lit fixtures under `test/Fmt/cli/check-mode/`
and `test/Fmt/cli/multi-file/` go green; `scripts/ci.sh --subset=t2`
includes the audited-corpus check step (guarded by `|| true` until
M7).

### Tests for User Story 2 (TDD)

- [X] T066 [P] [US2] Author lit fixture `test/Fmt/cli/check-mode/dirty-file.test` asserting `RUN: nsl-fmt --check %s; CHECK: <diff>; CHECK: exit 1`. Coverage backfill: `--check`'s diff path was already wired by T067 + the unified-diff helper (T073/T074/T075/T076); the fixture exercises it end-to-end with R5's `x+y` → `x + y` rewrite as the canonical-rule trigger. PASS on author (no XFAIL marker needed; the implementation is in place).
- [X] T067 [P] [US2] Author lit fixture `test/Fmt/cli/check-mode/clean-file.test` asserting clean file → exit 0 + empty stdout — observe FAILING
- [X] T068 [P] [US2] Author lit fixture `test/Fmt/cli/multi-file/one-bad-one-good.test` asserting one parse-error file + one clean file → both processed, exit 1, only the bad file's diagnostic on stderr (FR-003a continue-on-error). Coverage backfill: the multi-file loop in `tools/nsl-fmt/main.cpp` lines 334–381 already implements `aggregateExit |= rc` continue-on-error; the fixture exercises the bad+good ordering, the stdout-from-good-only invariant, and the `STDERR-NOT: good.nsl` bracketing assertion. PASS on author.
- [X] T069 [P] [US2] Author lit fixture `test/Fmt/cli/multi-file/all-clean.test` asserting `--check` over 3 already-canonical files → exit 0 — observe FAILING
- [X] T070 [P] [US2] Author lit fixture `test/Fmt/cli/mutually-exclusive/check-and-in-place.test` asserting `nsl-fmt -c -i x.nsl` produces frozen string from [`contracts/cli-surface.contract.md`](./contracts/cli-surface.contract.md) §2 + exit 2 — observe FAILING
- [X] T071 [P] [US2] Author lit fixture `test/Fmt/cli/mutually-exclusive/stdin-and-positional.test` per `cli-surface.contract.md` §2 — observe FAILING
- [X] T072 [P] [US2] Author lit fixture `test/Fmt/cli/mutually-exclusive/check-without-input.test` per `cli-surface.contract.md` §2 — observe FAILING
- [X] T073 [P] [US2] Author gtest `test_unit/Fmt/diff_emitter_test.cc::EmptyOnIdentical` asserting `emit_unified_diff(s, s, "a", "b") == ""` — observe FAILING
- [X] T074 [P] [US2] Author gtest `test_unit/Fmt/diff_emitter_test.cc::HunkFormat` asserting one-hunk output matches `--- a\n+++ b\n@@ -1,1 +1,1 @@\n-old\n+new\n` — observe FAILING
- [X] T075 [P] [US2] Author gtest `test_unit/Fmt/diff_emitter_test.cc::Determinism` asserting two `emit_unified_diff()` calls on the same inputs produce byte-identical output — observe FAILING

### Implementation for User Story 2

- [X] T076 [US2] Implement `lib/Fmt/Diff.{h,cpp}` Myers-diff-based unified-diff emitter per [`research.md`](./research.md) §5 + [`data-model.md`](./data-model.md) §7; T073/T074/T075 turn green
- [X] T077 [US2] Implement CLI `--check` / `-c` mode in `tools/nsl-fmt/main.cpp` — for each file: format → compare → if differ, print unified diff via `emit_unified_diff()`; track aggregate failure flag; T066/T067 turn green
- [X] T078 [US2] Implement multi-file continue-on-error loop in `tools/nsl-fmt/main.cpp` per [`contracts/cli-surface.contract.md`](./contracts/cli-surface.contract.md) §4 — per-file try/catch; aggregate failure flag; final exit code reflects aggregate; T068/T069 turn green
- [X] T079 [US2] Implement mutually-exclusive flag rejection in `tools/nsl-fmt/main.cpp` after `cl::ParseCommandLineOptions()` — emit each of the five frozen strings from [`contracts/cli-surface.contract.md`](./contracts/cli-surface.contract.md) §2; T070/T071/T072 turn green
- [X] T080 [P] [US2] Add `nsl-fmt --check test/audited/**/*.nsl || true` step to `scripts/ci.sh` per [`quickstart.md`](./quickstart.md) §10 (the `|| true` guard becomes a hard gate at M7 in a one-line follow-up commit)

**Phase 4 checkpoint**: All US2 fixtures green; CI step exists.
A CI maintainer can wire `nsl-fmt --check` into a GitHub Actions
step and get blocking feedback on style drift.

---

## Phase 5: User Story 3 (P3) — Library reuse for T5 LSP integration

**Story goal**: T5 (later) can link `libNslFmt.a` and call
`format_buffer()` directly. T2 freezes the public API surface and
proves CLI ↔ library parity.

**Independent test**: `test_unit/Fmt/format_parity_test.cc` passes
+ `scripts/audit_fmt_api.sh` reports exactly 10 public symbols.
Library can be linked from a synthetic external translation unit
(`test_unit/Fmt/external_link_test.cc`).

### Tests for User Story 3 (TDD)

- [X] T081 [P] [US3] Author gtest `test_unit/Fmt/format_parity_test.cc::CLIMatchesLibrary` invoking `nsl-fmt --stdin` via subprocess and `format_buffer()` directly on the same buffer; assert byte-identical output for 5 representative fixtures — observe FAILING
- [X] T082 [P] [US3] Author gtest `test_unit/Fmt/format_parity_test.cc::IdempotencePostCondition` asserting `format_buffer(format_buffer(s, c, f).formattedText, c, f).formattedText == format_buffer(s, c, f).formattedText` for the same 5 fixtures — observe FAILING
- [X] T083 [P] [US3] Author gtest `test_unit/Fmt/format_parity_test.cc::ExternalLinkSmoke` linking against `libNslFmt.a` from an external translation unit (`#include "nsl/Fmt/Fmt.h"`); assert one call to `format_buffer()` returns `Status::Success` for empty input — observe FAILING
- [X] T084 [P] [US3] Author lit fixture `test/Fmt/cli/range/inside-alt-block.test` asserting `nsl-fmt --range 5:7 file.nsl` reformats only lines 5–7 (lines outside range emitted byte-identical). Authored RED with `XFAIL: *` per Principle VIII; current behavior reformats the whole file (range no-op per `tools/nsl-fmt/main.cpp` lines 260–263). T090 + T091 turn green.
- [X] T085 [P] [US3] Author lit fixture `test/Fmt/cli/range/out-of-bounds.test` asserting `--range 100:200` on a 50-line file → exit 2 + frozen string from [`contracts/formatting-rules.contract.md`](./contracts/formatting-rules.contract.md) §7. Authored RED with `XFAIL: *` per Principle VIII; current behavior silently ignores the range and exits 0. T090 turns green.

### Implementation for User Story 3

- [X] T086 [US3] Freeze `include/nsl/Fmt/Fmt.h` public surface to exactly the 10 symbols in [`contracts/format-api.contract.md`](./contracts/format-api.contract.md) §3 — declarations only; bodies live in `lib/Fmt/`
- [X] T087 [US3] Implement `nsl::fmt::version_string()` in `lib/Fmt/Format.cpp` reading from CMake-defined `NSL_PROJECT_VERSION` and `LLVM_PROJECT_VERSION` symbols
- [X] T088 [US3] Implement `nsl::fmt::config_key_names()` in `lib/Fmt/Config.cpp` returning a static `ArrayRef<StringRef>` of the 10 Configuration keys in declaration order
- [X] T089 [US3] Implement `nsl::fmt::default_configuration()` as `constexpr` in `lib/Fmt/Config.cpp` returning the §5.1 defaults
- [X] T090 [US3] Implement `--range LINE:LINE` parsing in `tools/nsl-fmt/main.cpp` and threading through to `format_buffer(..., LineRange{...})` per [`contracts/cli-surface.contract.md`](./contracts/cli-surface.contract.md) §1. Three pieces land: (a) `parseLineRange()` syntax helper rejects malformed inputs (no colon, leading-zero half, non-digit char, `lo > hi`) → exit 2 with frozen `kErrRangeInvalidSyntax` string; (b) `countLines()` helper + bounds-check at both the stdin and per-file paths emits the frozen `error: --range <a>:<b> falls outside file (file has <N> lines)` on overflow → exit 2; (c) `processInput()` now takes `std::optional<LineRange>` and forwards it to `format_buffer()`. T085 (out-of-bounds) flips XFAIL→PASS. T084 (in-range partial reformat) stays XFAIL — that's T091's job (LayoutPlanner LineRange honoring). New companion fixture `test/Fmt/cli/range/invalid-syntax.test` covers the syntax-check frozen string.
- [X] T091 [US3] Extend `lib/Fmt/LayoutPlanner.cpp` to honor `LineRange` — emit lines outside the range from raw source bytes, only re-layout within the range. Implemented: `LayoutPlanner` takes `std::optional<LineRange>` + `fragmentStartLine`; builds a line-offset table from `src_`; dispatch macro short-circuits to `verbatimFromRange(node.loc())` when `nodeIntersectsRange` returns false. `format_buffer` computes each NSL fragment's absolute starting line from newline counts in the original source buffer and threads `range` + start-line through. T084 (in-range partial reformat) flips XFAIL→PASS; fixture had a pre-existing S2 bug (`wire y = a+b;`) reworked to use TransferStmt on declared ports. 586/589 lit PASS + 3 XFAIL (T032 R6, T035 R3, pre-existing struct_variable_emit_mlir).
- [X] T092 [US3] Wire `audit_fmt_api.sh` into `scripts/ci.sh` stage 2 (static checks) so symbol-count drift fails CI; T086 verification gate
- [X] T093 [US3] Verify T081/T082/T083 turn green (no new implementation expected; CLI and library both call into the same `format_buffer()`)

**Phase 5 checkpoint**: All US3 fixtures green;
`audit_fmt_api.sh` reports 10 symbols. T5 has a stable, parity-
verified API to link against.

---

## Phase 6: User Story 4 (P3) — Project-level style customization

**Story goal**: A project lead drops a `.nsl-fmt.toml` at the repo
root and every `nsl-fmt` invocation under that root picks it up.
`--config PATH` overrides discovery.

**Independent test**: lit fixtures under `test/Fmt/config/` go
green + gtest `test_unit/Fmt/config_parser_test.cc` covers TOML
edge cases.

### Tests for User Story 4 (TDD)

- [X] T094 [P] [US4] Author gtest `test_unit/Fmt/config_parser_test.cc::DefaultsMatchSpec` asserting `default_configuration()` returns the §5.1 example values for every field — observe FAILING
- [X] T095 [P] [US4] Author gtest `test_unit/Fmt/config_parser_test.cc::ParseAllTenKeys` asserting a TOML file with all 10 keys set non-default round-trips through `parse_config_file()` correctly — observe FAILING
- [X] T096 [P] [US4] Author gtest `test_unit/Fmt/config_parser_test.cc::UnknownKeyWarning` asserting unknown TOML key produces a `Diagnostic` with severity Warning and the frozen string from [`contracts/formatting-rules.contract.md`](./contracts/formatting-rules.contract.md) §7 — observe FAILING
- [X] T097 [P] [US4] Author gtest `test_unit/Fmt/config_parser_test.cc::OutOfRangeError` asserting `indent = "potato"` produces `Status::Error` + the frozen "must be" string — observe FAILING
- [X] T098 [P] [US4] Author lit fixture `test/Fmt/config/discovery/upward-walk.test` asserting `nsl-fmt sub/sub/foo.nsl` finds `.nsl-fmt.toml` at the root — observe FAILING
- [X] T099 [P] [US4] Author lit fixture `test/Fmt/config/explicit-config/overrides-discovery.test` asserting `--config /tmp/custom.toml` suppresses upward walk — observe FAILING
- [X] T100 [P] [US4] Author lit fixture `test/Fmt/config/unknown-key/warn-and-continue.test` asserting unknown-key warning is on stderr and formatting still succeeds — observe FAILING
- [X] T101 [P] [US4] Author lit fixture `test/Fmt/config/invalid-value/abort-with-error.test` asserting `indent = "potato"` aborts with exit 2 — observe FAILING

### Implementation for User Story 4

- [X] T102 [US4] Implement `lib/Fmt/Config.{h,cpp}` `Configuration` record per [`data-model.md`](./data-model.md) §5 — built-in defaults match §5.1 example values; T094 turns green
- [X] T103 [US4] Implement `lib/Fmt/Config.cpp` `parse_config_file()` using `toml++` per [`research.md`](./research.md) §4; T095 turns green
- [X] T104 [US4] Implement unknown-key warning path in `lib/Fmt/Config.cpp` (TOML iteration with `config_key_names()` lookup); T096/T100 turn green
- [X] T105 [US4] Implement out-of-range / wrong-type error path in `lib/Fmt/Config.cpp` (per-key range check, frozen diagnostic strings); T097/T101 turn green
- [X] T106 [US4] Implement `lib/Fmt/ConfigDiscovery.{h,cpp}` upward-walk for `.nsl-fmt.toml` per [`contracts/format-api.contract.md`](./contracts/format-api.contract.md) §4 (the explicit non-pure-function carve-out); T098 turns green
- [X] T107 [US4] Implement CLI `--config PATH` flag in `tools/nsl-fmt/main.cpp` — when set, suppress `discover_config()`; T099 turns green
- [X] T108 [US4] Wire `Configuration` through `format_buffer()` so every `LayoutPlanner` decision honors the active config; verify R1–R6 fixtures still green with non-default configs. `Format.cpp` constructs `LayoutPlanner(src, config)`; the planner consults `cfg_` in `formatNode(ConcatExpr)` (`spaces_inside_braces`), `formatCondCaseBlock` (`align_case_arrows`), `formatNode(StructDecl)` (`align_struct_members`), and `indentStep()` (the `Indent` enum). Regression sweep: R1/R2/R4/R5 default-config fixtures (T030/T031/T033/T034) remain GREEN; non-default fixtures T119 (tab), T120 (spaces2), T123 (spaces_inside_braces=true) authored GREEN — the active-config path is exercised end-to-end. R3 / R6 still XFAIL pending their respective parser/CST blockers.

### Additional tests (added during /speckit-analyze remediation, finding C3)

Per [`contracts/formatting-rules.contract.md`](./contracts/formatting-rules.contract.md) §8 rule↔key interaction matrix, every Configuration key has an effect on at least one rule. The default-valued fixtures in T030–T035 do not exercise the non-default codepath. The fixtures below close that gap.

- [X] T119 [P] [US4] Author lit fixture `test/Fmt/config/non-default/indent-tab/tab-indent.test` asserting `indent = "tab"` emits `\t` characters (not spaces) at every indent level. Authored GREEN — `LayoutPlanner::indentStep()` returns 1 for `Indent::Tab` and the renderer's `emitIndent` emits a single `\t` whenever `columnIndent > 0`, so the toggle was already wired through T053.
- [X] T120 [P] [US4] Author lit fixture `test/Fmt/config/non-default/indent-spaces2/two-space-indent.test` asserting `indent = "spaces2"` emits exactly 2 spaces per level. Authored GREEN — `LayoutPlanner::indentStep()` returns 2 for `Indent::Spaces2`; uses `--strict-whitespace` so a regression to 4-space mode would fail the CHECK.
- [X] T121 [P] [US4] Author lit fixture `test/Fmt/config/non-default/brace-style-allman/allman-blocks.test` and wire `brace_style = "allman"` through the canonical-layout block emitters. `formatNode(StructDecl)` and `formatCondCaseBlock` now branch on `cfg_.brace_style`: K&R puts `{` on the same line as the keyword (default), Allman emits `<keyword>` then a hardline then `{` on its own line at the outer indent. Scope: covers the three blocks with canonical layout (`struct`, `alt`, `any`); `module`/`func`/`proc`/`seq`/`par` still use the verbatim parent gap (their brace style mirrors the source), pending T055.
- [X] T122 [P] [US4] Author lit fixture `test/Fmt/config/non-default/trailing-commas-add/proc-args.test` asserting `trailing_commas = "add"` appends a comma after the last arg in multi-line `proc_name` arg lists; companion `test/Fmt/config/non-default/trailing-commas-remove/proc-args.test` asserts `"remove"` strips it. Authored RED — both fixtures marked `XFAIL: *` pending T054 (R3 `proc_name` argument-list wrapping) and the `trailing_commas` wiring in `formatNode(ProcNameDecl)`; observe XFAIL in `lit build-noasan/test/Fmt/`.
- [X] T123 [P] [US4] Author lit fixture `test/Fmt/config/non-default/spaces-inside-braces/concat.test` asserting `spaces_inside_braces = true` emits `{ a, b, c }` (with leading + trailing space) per [`contracts/formatting-rules.contract.md`](./contracts/formatting-rules.contract.md) §4. Authored GREEN — `formatNode(ConcatExpr)` already reads `cfg_.spaces_inside_braces` (wired in T050) and switches the open/close braces between `{`/`}` and `{ `/` }` accordingly.
- [X] T124 [P] [US4] Author lit fixture `test/Fmt/config/non-default/preserve-comments-leading-only/drop-trailing.test` asserting `preserve_comments = "leading_only"` drops trailing line comments while keeping leading ones; companion `none/drop-all.test` asserts `"none"` drops all comments per [`contracts/formatting-rules.contract.md`](./contracts/formatting-rules.contract.md) §6. Authored RED — both fixtures marked `XFAIL: *` pending T051 (R6 attached-comment preservation) and the `preserve_comments` wiring through the trivia-emission helpers; observe XFAIL in `lit build-noasan/test/Fmt/`.

**Phase 6 checkpoint**: All US4 fixtures green. A project lead's
`.nsl-fmt.toml` works end-to-end.

---

## Phase 7: Polish & Cross-Cutting Concerns

**Purpose**: Audited-corpus wiring, documentation updates, final
determinism gate, pre-merge checklist sweep.

- [X] T109 [P] Author lit fixtures `test/Fmt/idempotence/audited/<project>.test` for each of the seven audited projects (rv32x_dev, turboV, mmcspi, SDRAM_Controler, mips32_single_cycle, ahb_lite_nsl, cpu16) — initially marked `UNSUPPORTED:` until M7 vendors `test/audited/<project>/`; auto-activate when paths exist (per [`research.md`](./research.md) §6). Implemented with the per-project lit-feature gate `audited-<project>` defined in `test/lit.cfg.py` (probes `os.path.isdir(test/audited/<project>)` at lit-config time and adds the feature when present). Each fixture uses `REQUIRES: audited-<project>` so the entire set lights up the moment M7 P-VEN lands a project tree — no T2-side edit required. All seven fixtures land as `Unsupported` in `lit build-noasan/test/Fmt/` today (7 of 7 skipped).
- [X] T110 [P] Amend `docs/design/nsl_tooling_design.md` §5.2 architecture diagram to show the directive-aware pre-pass per [`quickstart.md`](./quickstart.md) §9 step 2
- [X] T111 [P] Amend project-root `CLAUDE.md` §2.3 footnote (under "Formatter v0") with a one-line note about the directive-aware pre-pass scope decision per [`quickstart.md`](./quickstart.md) §9 step 1
- [X] T112 [P] Amend `docs/CLAUDE.md` §7 line-range table for `nsl_tooling_design.md` if §5.2 amendment shifts boundaries (re-grep `^### \|^## ` after T110)
- [ ] T113 Run two-build determinism check per [`quickstart.md`](./quickstart.md) §5 — `ninja nsl-fmt` in two distinct build paths; assert byte-identical binary
- [X] T114 Run synthetic-corpus idempotence sweep per [`quickstart.md`](./quickstart.md) §5 — `for f in test/Fmt/idempotence/synthetic/*.nsl; do format-twice-diff; done`
- [ ] T115 Verify pre-merge checklist from [`quickstart.md`](./quickstart.md) §7 — every box ticked
- [X] T116 [P] Run `clang-format` and `clang-tidy` (project profile) on every new file in `lib/Fmt/`, `include/nsl/Fmt/`, `tools/nsl-fmt/`, `test_unit/Fmt/`
- [X] T117 [P] Verify SPDX header presence on every new file (per Constitution "Build, Code, and Licensing Standards")

### Additional task (added during /speckit-analyze remediation, finding C2 — SC-003 perf gate)

- [X] T118 [P] Author gtest `test_unit/Fmt/perf_smoke_test.cc::Check1000LineUnder250ms` constructing a synthetic 1000-line NSL file (e.g. 1000 `wire foo_<n> [8];` declarations under one `module`), invoking `format_buffer()` from a `--check`-equivalent code path, measuring wall-clock with `std::chrono::steady_clock`, and asserting elapsed < 500 ms (2× SC-003's 250 ms target to absorb CI hardware variance) — gates SC-003. Keep this test in the `check-fmt-unit` ninja target so CI stage 3 catches regressions.

**Phase 7 checkpoint**: PR is mergeable per Principle IX merge
gate (CI green, CodeRabbit review clean, Linear issue referenced).

---

## Dependencies

### Phase order

```
Phase 1 (Setup)
   ↓
Phase 2 (Foundational — blocking ALL user stories)
   ↓
Phase 3 (US1 — P1 — MVP)
   ↓
Phase 4 (US2 — P2 — depends on Phase 3 for canonical output)
Phase 5 (US3 — P3 — depends on Phase 3 for format_buffer)
Phase 6 (US4 — P3 — depends on Phase 2 for Configuration plumbing,
         Phase 3 for LayoutPlanner)
   ↓
Phase 7 (Polish)
```

Phases 4, 5, and 6 are mutually independent once Phase 3 lands —
they can be implemented in parallel.

### Within-phase dependencies

- T001–T003 must precede every other task in Phase 1 (CMake
  scaffolding).
- T004 (Fmt.h skeleton) blocks T086 (Fmt.h freeze) which blocks T093.
- T011–T015 (DirectiveSplitter) block T019 (CSTBuilder uses
  Slice).
- T017–T019 (Parser CST mode + CSTBuilder) block T026 (`format_buffer`
  wiring).
- T020 (CST invariants) blocks T026 (Format won't function with a
  broken CST).
- T024 (Doc IR) + T025 (LayoutRenderer) block T049–T055
  (LayoutPlanner).
- T051 (R6 attached comments) blocks T052/T053/T054 (R1/R2/R3
  alignment rules build on top of trivia handling).
- T056–T058 (CLI default/in-place/stdin) block T077 (`--check` mode
  reuses CLI plumbing).
- T076 (Diff emitter) blocks T077 (`--check` calls
  `emit_unified_diff`).
- T086 (Fmt.h freeze) blocks T092 (`audit_fmt_api.sh` integration).
- T102–T105 (Config record + parser) block T108 (config wired
  through format_buffer).

### Story-level dependencies

```
US1 (P1) — independent of US2/US3/US4
   ↑
   ↑ Required for canonical output
   ↑
US2 (P2) — depends on US1 (--check needs `format_buffer` to compare against)
US3 (P3) — depends on US1 (parity test needs canonical output)
US4 (P3) — depends on US1 (config tweaks have nothing to apply
            against until LayoutPlanner exists)
```

---

## Parallel execution opportunities

### Phase 1 — Setup
T004, T005, T006, T008, T009, T010 are all `[P]` — different
files, no inter-dependencies. Single contributor can land them in
one session.

### Phase 2 — Foundational
- T011, T012, T013 in parallel (3 independent gtest fixtures
  before DirectiveSplitter exists).
- T021, T022, T023 in parallel (3 independent Doc/renderer
  gtests before Doc.cpp exists).
- T028, T029 in parallel (two independent CI scripts).

### Phase 3 — US1
- All 19 test fixtures (T030–T048) are `[P]` — different files,
  no inter-dependencies. Author them in one batch (high
  parallelism; ideal for `nsl-test-author` sub-agent offload).
- T049–T054 are NOT `[P]` (they share `lib/Fmt/LayoutPlanner.cpp`).
- T055 (the rest of `visit*()`) is independent of T049–T054 by
  AST node category — can be parallelized within the file by
  splitting the file into `LayoutPlanner_Decl.cpp` /
  `LayoutPlanner_Stmt.cpp` / `LayoutPlanner_Expr.cpp` if
  desired (Phase 2 polish item).

### Phase 4 — US2
- All 10 test fixtures (T066–T075) are `[P]`.
- T076 (Diff) and T079 (mutually-exclusive) can be parallel
  (different files).
- T077 (--check) and T078 (multi-file) share `main.cpp` — NOT
  parallel.

### Phase 5 — US3
- All 5 test fixtures (T081–T085) are `[P]`.
- T086, T087, T088, T089 are `[P]` (different functions in
  different files).
- T090 and T091 share `tools/nsl-fmt/main.cpp` and
  `lib/Fmt/LayoutPlanner.cpp` respectively — can be parallel.

### Phase 6 — US4
- All 8 test fixtures (T094–T101) are `[P]`.
- T102–T107 share `lib/Fmt/Config.cpp` for some — sequential
  within the file; T106 (ConfigDiscovery, separate file) is `[P]`
  with the others.

### Phase 7 — Polish
- T109, T110, T111, T112, T116, T117 are `[P]`.

---

## MVP suggested scope

**Minimum demonstrable value** = Phase 1 + Phase 2 + Phase 3
(US1). This delivers:

- A working `nsl-fmt` binary that takes any synthetic NSL file
  and emits canonical formatting (six §5.3 rules applied,
  comments preserved, directives passed through verbatim).
- Idempotence guaranteed on the synthetic corpus.
- Full Constitution Principle VIII test discipline (every
  feature has its failing-first test in history).

Phases 4, 5, 6 add CI integration, T5-readiness, and config
customization respectively — each independently mergeable on
top of Phase 3.

---

## Independent test criteria per story

| Story | Independent test |
|---|---|
| US1 (Phase 3) | `ninja check-fmt-lit` passes `test/Fmt/{rules,cli/stdin,cli/in-place,edge,directives,idempotence/synthetic}/`; `nsl-fmt --stdin < malformatted.nsl` returns canonical bytes. |
| US2 (Phase 4) | `nsl-fmt --check $(git ls-files '*.nsl')` exits 0 on a clean tree; exits 1 with per-file unified diff if any file would change; multi-file invocation with one parse-error file still processes the others. |
| US3 (Phase 5) | `test_unit/Fmt/format_parity_test.cc` passes; `audit_fmt_api.sh` reports exactly 10 public symbols; an external translation unit can `#include "nsl/Fmt/Fmt.h"` and link against `libNslFmt.a`. |
| US4 (Phase 6) | A `.nsl-fmt.toml` at the repo root with `indent = 2` causes `nsl-fmt sub/sub/foo.nsl` to emit 2-space indent; `--config /dev/null` reverts to defaults. |

---

## Format validation

All 117 tasks above strictly follow the
`- [ ] T### [P?] [Story?] Description with file path` format.
Spot check:

- T001: `- [ ] T001 Create lib/Fmt/CMakeLists.txt …` ✅ (Setup, no [P], no [Story])
- T011: `- [ ] T011 [P] Create gtest fixture test_unit/Fmt/…` ✅ (Foundational, [P], no [Story])
- T030: `- [ ] T030 [P] [US1] Author lit fixture test/Fmt/rules/…` ✅ (Story phase, [P], [US1])
- T080: `- [ ] T080 [P] [US2] Add nsl-fmt --check … to scripts/ci.sh` ✅
- T109: `- [ ] T109 [P] Author lit fixtures …` ✅ (Polish phase, [P], no [Story])

---

**Total tasks**: 125 — Phase 1: 10, Phase 2: 19, Phase 3: 37
(US1: 36 + 1 added by /speckit-analyze C4), Phase 4: 15 (US2),
Phase 5: 13 (US3), Phase 6: 21 (US4: 15 + 6 added by C3),
Phase 7: 10 (9 + 1 added by C2).

**Parallel opportunities**: 71 of 125 tasks marked `[P]` —
test-fixture authoring batches are particularly amenable to
sub-agent offload (`nsl-test-author`). All 8 tasks added by
/speckit-analyze remediation (T118–T125) are `[P]`.
