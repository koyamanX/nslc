<!-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception -->

# Phase 0 Research: T3 — `nsl-lsp` Skeleton

**Branch**: `010-t3-lsp-skeleton` | **Date**: 2026-05-05

This document records the technology / approach decisions taken
before Phase 1 design, with rationale and rejected alternatives.
Decisions are referenced by ID (`R1`, `R2`, …) from `plan.md`,
the contract files, and `tasks.md`.

---

## R1: JSON-RPC framing implementation

**Decision**: Implement the `Content-Length:`-framed JSON-RPC layer
in-tree as a small `JSONTransport` class (~200 LOC). Use
`llvm::json::Value` / `llvm::json::Object` / `llvm::json::Array`
(already a transitive dep via `nsl-driver`) for parsing and
serialization.

**Rationale**:

- The LSP base-protocol framing is a **single header line plus a
  `\r\n\r\n` separator**: `Content-Length: <N>\r\n\r\n<N bytes of
  JSON>`. The full grammar fits in ~50 lines of state-machine code
  on the read path and ~20 lines on the write path.
- `llvm::json` is already linked by every binary that depends on
  `nsl-driver` (the diagnostic engine emits JSON for the
  `--diagnostic-format=json` mode); pulling it into `nsl-lsp` adds
  zero new transitive dependencies.
- Vendoring clangd's `JSONTransport` was considered but rejected:
  clangd's transport is entangled with its `Notification` /
  `Reply` callback machinery, its `Cancelable` wrapper, and its
  `RPCStats` instrumentation. Excising those threads cleanly would
  produce ~80% of the in-tree implementation we'd write anyway,
  while adding a vendoring obligation.
- A from-scratch implementation also keeps ownership of the
  cancellation-token plumbing local to the LSP transport — clangd
  threads cancellation through its own request-id table, which we
  reproduce minimally per FR-020h–j.

**Alternatives rejected**:

- **Vendor clangd `JSONTransport`** — too entangled (see above).
- **`nlohmann/json`** — would add a new external dependency. No
  benefit over `llvm::json` for our small JSON surface.
- **`rapidjson`** — same as `nlohmann/json`. Fast but unjustified
  here; LSP message volume at T3 is "one request per keystroke,"
  not "one million events per second."

---

## R2: Concurrency primitives

**Decision**: `llvm::ThreadPool` for the worker pool consumed by
`TUScheduler`. `std::mutex` + `std::condition_variable` for
per-`NslTU` synchronization (matches the design-doc skeleton at
[`docs/design/nsl_tooling_design.md`](../../docs/design/nsl_tooling_design.md)
§3.3 verbatim). Default worker count = `min(std::thread::hardware_concurrency(), 4)`,
overridable via `NSL_LSP_WORKERS` (env var read once at startup).

**Rationale**:

- `llvm::ThreadPool` is already linked transitively. It provides
  task submission, futures, and graceful shutdown — everything the
  TUScheduler needs.
- Per-`NslTU` `std::mutex` + condvar matches the C++17 code shown
  in the design doc, eliminating any translation cost from "design
  doc" to "implementation". This is the simplest correct answer.
- `min(hwconc, 4)` is a conservative cap: the audited corpus is
  ≤ 7 files, and even on a 16-core CI runner there is no benefit
  to spawning more worker threads than there are likely-open
  documents. Capping at 4 also keeps SC-003 (determinism) easy to
  reason about — diagnostics for any one document are produced by
  exactly one worker per parse cycle, regardless of pool size.
- The `NSL_LSP_WORKERS` override exists for two reasons: (a)
  performance experimentation post-T3 without code changes; (b)
  setting workers=1 in the determinism-check CI cell to remove
  any conceivable scheduler nondeterminism contribution. Both
  uses are forward-looking; neither is a T3 acceptance scenario.

**Alternatives rejected**:

- **`std::async` + per-call thread** — wasteful; spawns a thread
  per parse cycle. Already-rejected pattern across the LLVM tree.
- **clangd's `AsyncTaskRunner`** — reproduces `llvm::ThreadPool`
  with extra structure not needed at T3.
- **Single-threaded server** — would technically satisfy the test
  gate (which involves only one document at a time), but blocks
  T4+ when concurrent feature requests across documents become
  the norm. Architectural shortcut not worth taking.

---

## R3: Integration test harness

**Decision**: In-tree gtest. A small reusable helper class
`LspSession` at `test/lsp/LspSession.{h,cpp}` spawns `nsl-lsp` as
a subprocess with `llvm::sys::ExecuteAndWait` (or its async
sibling), pipes JSON-RPC envelopes over stdin, and collects
responses + stderr from stdout / stderr pipes. Each test fixture
is a gtest `TEST_F` that constructs an `LspSession`, drives it, and
asserts on the captured output.

**Rationale**:

- gtest is already the project's unit-test driver per
  [`docs/design/nsl_compiler_design.md`](../../docs/design/nsl_compiler_design.md)
  §14 testing strategy. Reusing it for LSP integration tests
  imposes zero new harness onto the contributor.
- Subprocess-based testing exercises the full `nsl-lsp` binary,
  including the `runStdioServer` entry point and the JSON-RPC
  framing — the exact code path real editors use. In-process
  testing would skip the framing layer and the entry point.
- `llvm::sys::Process` / `llvm::sys::ExecuteAndWait` are already
  linked. No new dependency.
- Stderr capture is straightforward via the pipe — necessary for
  asserting on the FR-020d–g log records and SC-009 stderr-on-failure.

**Alternatives rejected**:

- **Python `pytest-lsp`** — adds a Python dependency to the test
  suite, complicates the CI cell, and produces output that
  doesn't integrate with ctest's built-in result reporting. The
  in-tree gtest harness gives us the same per-test output and
  the same "ctest --output-on-failure" UX we already have for
  every other layer.
- **In-process testing** (instantiate `NslLSPServer` directly,
  skip framing) — reduces test coverage; we'd need a separate
  test for the framing itself, and the integration "is the
  server actually wired up correctly?" question stays
  unanswered.
- **lit + FileCheck** — works for one-shot stdin → stdout text
  fixtures, but a stateful protocol exchange (initialize →
  didOpen → wait for diagnostic → didChange → wait for fresh
  diagnostic → shutdown) is awkward to express in a single
  RUN line.

---

## R4: Folding-range computation strategy

**Decision**: An `ASTVisitor`-derived class `FoldingRangeBuilder`
walks the parsed `CompilationUnit`. For each block-opening AST
node listed in FR-014 (`module`, `declare`, `func`, `proc`,
`state`, `seq`, `alt`, `any`, `par`, `if`/`else`, `for`, `while`,
`generate`, `_init`), it emits one `FoldingRange` if the node's
`SourceRange` spans ≥ 2 lines. For multi-line block comments
(FR-015), it post-processes the token-lattice (or token stream
from M1 lexer output) to find `/* … */` pairs spanning ≥ 2 lines.

**Rationale**:

- `ASTVisitor` is the M2-era visitor pattern already used for
  `Printer.h` and the M3 Sema passes. Reusing it costs nothing.
- Block-opener AST nodes already carry `SourceRange` (Principle
  IV); the start and end line numbers are the trivial mapping
  from `SourceRange.begin.line` and `SourceRange.end.line`.
- Block comments are not in the AST (they're trivia). Reading
  them from the M1 token stream is the cheapest path; the M1
  lexer's comment tokens carry `SourceLocation`.
- Parse errors leave a partial AST whose children still carry
  `SourceRange` — FR-017 ("does not crash on parse error") is
  satisfied by walking whatever the parser's error-recovery
  produced.

**Alternatives rejected**:

- **Re-tokenize specifically for folding** — wasteful; the AST
  already has the structure. Token-level folding would also
  duplicate the block-recognition logic the parser already
  performs.
- **CST walk** — at T3 the CST layer (per
  [`docs/design/nsl_tooling_design.md`](../../docs/design/nsl_tooling_design.md)
  §2.4) is not yet implemented; introducing it for folding
  alone is outsized. The CST lands when the formatter (T2/T5)
  needs it.

---

## R5: Diagnostic mapping seam

**Decision**: A free function
`nsl::lsp::toLspDiagnostic(const Diagnostic& d, const SourceManager& sm) -> llvm::json::Value`
in `lib/LSP/DiagnosticMapper.{h,cpp}`. Reads
`Diagnostic.severity`, `Diagnostic.loc`, `Diagnostic.message`,
`Diagnostic.notes`, and `Diagnostic.is_include_from_note` from the
existing M1/M3 `DiagnosticEngine` API. Produces an LSP `Diagnostic`
object whose:
- `range` comes from `loc` via `byteOffsetToLspRange` (R6);
- `severity` maps `Note→Information(3)`, `Warning→Warning(2)`, `Error→Error(1)`;
- `code` comes from a small lookup table keyed on the diagnostic
  message prefix (e.g., `S01:`, `S02:`, …) — see
  [`contracts/diagnostic-mapping.contract.md`](./contracts/diagnostic-mapping.contract.md);
- `source` is `"nsl-sema"`, `"nsl-parse"`, or `"nsl-preprocess"`
  depending on the diagnostic's origin (determined from a small
  origin-tagging helper added to `DiagnosticEngine`);
- `relatedInformation` materializes any `is_include_from_note`
  notes per LSP `DiagnosticRelatedInformation`.

**Rationale**:

- The existing `Diagnostic` struct already carries everything we
  need (severity, loc, message, notes, include-from flag); no API
  widening is needed in M1/M3.
- A free function (rather than a method on `Diagnostic`) keeps the
  LSP-specific code out of `nsl-basic`'s public surface — the
  Principle II layered architecture rule. Only `lib/LSP/` knows
  about LSP types.
- The lookup table for `code` keying is small (~30 entries — one
  per `Sn` with a diagnostic, plus a handful of parser/preprocessor
  notes) and lives in the contract file so the mapping is
  reviewable in one place.

**Alternatives rejected**:

- **Method on `Diagnostic`** — pollutes `nsl-basic` with LSP types;
  Principle II violation.
- **Subclassing `Diagnostic`** — overkill; adds boilerplate for no
  benefit.
- **Auto-derive `code` from the message text** — fragile (any wording
  change to a diagnostic breaks the LSP code stability). The lookup
  table makes the binding explicit and testable.

---

## R6: Position encoding (byte ↔ UTF-16)

**Decision**: A pair of free functions in
`lib/LSP/PositionEncoding.{h,cpp}`:

- `byteOffsetToLspPosition(StringRef line, size_t byteOffset) -> {line: uint32, character: uint32}` —
  walks the UTF-8 line, counting UTF-16 code units, until the byte
  offset is reached.
- `lspPositionToByteOffset(StringRef line, uint32 character) -> size_t` —
  inverse.

For pure ASCII lines (the audited-corpus norm), both are O(1)
short-circuits because UTF-16 code-unit count = byte count. For
non-ASCII lines, both are O(N) in the line length.

**Rationale**:

- The conversion is local: each LSP `Position` is a
  `(line, character)` tuple where `line` indexes into the source
  document split at `\n` and `character` indexes UTF-16 code units
  within that line. The conversion never crosses a `\n`.
- Keeping the conversion behind a named pair of functions
  (rather than inlined into the diagnostic mapper) lets the unit
  test pin the conversion behavior independently of the
  diagnostic-mapping logic.
- The ASCII short-circuit is what makes the seam free in the
  common case — the audited corpus contains zero non-ASCII NSL
  source as of P-VEN's expected vendoring boundary.

**Alternatives rejected**:

- **UTF-16 internally** — would force `libNSLFrontend.a` to switch
  off byte offsets, an enormous cross-cutting change unjustified
  by T3's scope.
- **`positionEncodings` capability negotiation** — a 3.17 feature,
  out of scope per the LSP 3.16 floor (Q3).
- **ICU library dependency** — overkill; UTF-8 → UTF-16 code-unit
  counting is ~30 lines without a library.

---

## R7: Cancellation token primitive

**Decision**: A small `CancellationToken` struct holding
`std::shared_ptr<std::atomic<bool>> cancelled` plus a non-owning
pointer to the per-request entry in the `NslLSPServer`'s in-flight
table. The protocol layer hands a fresh token to each cancellable
request handler at dispatch time; on `$/cancelRequest`, the
protocol layer flips the corresponding atomic.

**Rationale**:

- `std::atomic<bool>` is the lightest possible token: one byte of
  state, zero allocation overhead, lock-free reads on every
  reasonable platform.
- `std::shared_ptr` lets the token outlive the handler if the
  protocol layer needs to reference it after dispatch (e.g., to
  emit the `RequestCancelled` response after the worker
  acknowledges) without lifetime concerns.
- The `FoldingRangeBuilder` polls the token at coarse granularity
  — once per visited block-opener AST node (per FR-020i: "between
  AST traversal subtrees"). Polling at finer granularity would be
  wasteful; coarser would risk SC-010's 200 ms budget on
  pathologically deep ASTs.

**Alternatives rejected**:

- **`std::stop_token` (C++20)** — the right primitive in spirit,
  but C++20 is forbidden per the Build/Code/Licensing standards.
- **clangd's `Context` cancellation** — entangled with clangd's
  thread-local state machinery; over-engineered for one
  cancellable request type.

---

## R8: Test-fixture authoring strategy

**Decision**: For each of the 23 `Sn` constraints with a
diagnostic-string contract (see
[`specs/006-m3-sema/contracts/diagnostic-string.contract.md`](../006-m3-sema/contracts/diagnostic-string.contract.md)),
one fixture file under `test/lsp/fixtures/s<NN>_<short_name>.nsl`
with one diagnostic-emitting line. The locked diagnostic string is
the same one M3 already asserts; the fixture's expected
`publishDiagnostics` payload is computed from that string plus the
position of the offending source. **Fixtures are NOT generated**
for T3 — they are authored by hand, mirroring the layout used by
`test/sema/s01/` … `test/sema/s29/`. This keeps the LSP test
fixtures readable and avoids introducing a new generator script.

**Rationale**:

- The 23 fixtures × ~10 LOC each = ~230 LOC of fixture text;
  hand-authoring is fast and the result is reviewable.
- A generator would be tempting (parse the M3 diagnostic-string
  contract, emit one fixture per row) but the marginal benefit is
  small and the cost (a new generator script + its own test) is
  not.
- Folding-range fixtures (~5) and cancellation fixtures (~2) are
  also hand-authored.

**Alternatives rejected**:

- **Generator script reading the M3 contract** — overkill at this
  fixture count.
- **Re-derive fixtures from existing `test/sema/s<NN>/` files** —
  those files have FileCheck `RUN` lines and `// expected-error`
  comments tightly coupled to the lit driver; the LSP fixtures
  need pure-NSL files without those annotations. Reusing them
  would require a `sed`-pipeline that's more brittle than just
  authoring fresh.

---

## R9: Server output for capability assertion (SC-008)

**Decision**: SC-008 (capability advertisement is exact) is
verified by a dedicated `lifecycle_test::CapabilitiesExact` test
case that:

1. Starts the server.
2. Sends `initialize`.
3. Compares the response's `result.capabilities` against the
   frozen JSON object in
   [`contracts/lsp-protocol.contract.md`](./contracts/lsp-protocol.contract.md)
   §1.2 (the "exact capabilities advertisement" subsection).
4. Asserts byte-equality on the JSON serialization (after
   canonicalization with `llvm::json::OStream` set to
   pretty-print mode = false, sorted-keys mode).

**Rationale**:

- Byte-equality on canonical JSON serialization makes any
  unintended capability addition fail the test loudly.
- Per Principle VII coupling, every later T-track milestone that
  adds a capability MUST update the contract file in the same PR
  that adds the implementation; the test is the mechanism.

**Alternatives rejected**:

- **Substring matching** — would let a typo in capability shape
  pass silently.
- **Schema validation** — overkill for a single fixed object; the
  byte-equality form is simpler and stricter.
