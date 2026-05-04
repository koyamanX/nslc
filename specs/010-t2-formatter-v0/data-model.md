<!-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception -->

# Phase 1 Data Model: T2 — Formatter v0 (`nsl-fmt`)

**Branch**: `010-t2-formatter-v0` | **Date**: 2026-05-04
**Plan**: [plan.md](./plan.md) | **Research**: [research.md](./research.md)

This document catalogs the entities `libNslFmt.a` introduces and
the relationships between them. Each entity has a stable name, a
declared file location, an invariants list, and a list of which
spec FR / SC it satisfies. The contracts under
[`contracts/`](./contracts/) freeze the public-surface subset of
this catalog.

---

## §1. CST entities (`lib/Fmt/CST.{h,cpp}`)

```text
CSTNode (class)
├── DirectiveTok          (leaf)  — opaque preprocessor directive line
├── NSLToken              (leaf)  — one source token (tok::Kind + lexeme + SourceRange)
├── Trivia                (leaf)  — whitespace / comment / newline between tokens
└── CSTNode               (interior) — children: vector<CSTNode*>; preserved trivia ranges
```

**Type details**:

| Field | Type | Invariants |
|---|---|---|
| `CSTNode::id` | `NodeID` (opaque uint64) | unique within a single parse; stable across reparses if input unchanged |
| `CSTNode::range` | `basic::SourceRange` | NEVER `{}` — every node has a precise byte range (Principle IV) |
| `CSTNode::children` | `llvm::SmallVector<CSTNode*, 4>` | child order matches source order; no dropped children (Principle I — no silent AST drops; CST extends this rule) |
| `CSTNode::leadingTrivia` / `trailingTrivia` | `std::vector<Trivia>` | empty vector is allowed (no trivia present); never `nullptr` |
| `Trivia::kind` | `enum class { Whitespace, LineComment, BlockComment, Newline }` | one variant per source character category |
| `Trivia::text` | `llvm::StringRef` | byte-for-byte copy from source buffer; lifetime tied to the `MemoryBuffer` |
| `DirectiveTok::opcode` | `enum class { Include, Define, Undef, Ifdef, Ifndef, If, Else, Endif, Line }` | one of the nine `pp.ebnf` §2 directive kinds |
| `DirectiveTok::rawText` | `llvm::StringRef` | the entire directive line (or multi-line `\`-continued span) byte-for-byte |
| `NSLToken::kind` | `nsl::lex::TokenKind` (existing M1 enum) | reuses the M1 token taxonomy verbatim — Principle II |

**Relationships**:
- `CSTNode` is built by `CSTBuilder` (which extends `nsl::parse::Parser`
  via the `emitCST_` flag — research §2). One `CSTNode` per AST node;
  the AST and CST are *parallel* trees with the same node count and
  ranges.
- `DirectiveTok` instances are NEVER children of an interior `CSTNode`
  that represents an NSL grammar production; they sit at the
  *top level* of the file's CST (siblings of NSL fragments).
- `Trivia` is attached to its preceding (`trailingTrivia`) or
  succeeding (`leadingTrivia`) token per the design-doc §2.4 rule;
  ambiguous trivia (a comment between two tokens) attaches as
  `trailingTrivia` of the earlier one.

**Spec mapping**:
- FR-009 / FR-010 (preserve attached comments) → `Trivia` invariants.
- FR-012a (raw-source parse + opaque directives) → `DirectiveTok`.
- FR-011 (preserve numeric literal forms) → `NSLToken::lexeme` is
  byte-for-byte from source.
- Principle I (no silent AST drops) → `CSTNode::children` invariant.

---

## §2. Directive splitter (`lib/Fmt/DirectiveSplitter.{h,cpp}`)

```cpp
class DirectiveSplitter {
public:
    struct Slice {
        enum class Kind { Directive, NSLFragment };
        Kind             kind;
        basic::SourceRange range;
        llvm::StringRef    rawText;
    };

    std::vector<Slice> split(llvm::StringRef sourceBuffer,
                             basic::FileID  fileID);
};
```

**Invariants**:
- `split()` is a pure function of `sourceBuffer` (no environment
  reads, no time, no random) — Principle V byte-stability.
- For every byte in `sourceBuffer`, exactly ONE `Slice` covers it
  (no gaps, no overlaps).
- `Slice::range` is monotonically non-decreasing across the result
  vector (source order preserved).
- A `Slice::Kind::Directive` covers a complete directive line,
  including all `\`-continued continuation lines (per `pp.ebnf` §2.2).
- BOM bytes at file start (if present) appear as the leading trivia
  of the first `Slice`, not as a separate slice.
- CRLF line endings are scanned as single newlines; the original
  bytes are preserved in `rawText` until the renderer normalises
  them (per spec edge-cases entry).

**Spec mapping**:
- FR-012a (the formatter parses raw source *before* preprocessing).
- §1 of [research.md](./research.md) (the implementation choice).

---

## §3. CST builder (`lib/Fmt/CSTBuilder.{h,cpp}`)

```cpp
class CSTBuilder {
public:
    CSTBuilder(nsl::parse::Parser &parser);

    void beginNode(CSTNode::Kind kind, basic::SourceLoc start);
    void recordToken(nsl::lex::Token tok, std::vector<Trivia> &&leading);
    void endNode(basic::SourceLoc end);

    std::unique_ptr<CSTNode> takeRoot();
};
```

**Invariants**:
- `CSTBuilder` is owned by the `Parser` for the duration of one
  parse; transferred out via `takeRoot()` afterwards.
- Every `Parser::parseProduction()` call sandwiches its body
  with `beginNode()` / `endNode()` (one CST node per AST node).
- Trivia between tokens is collected by the lexer's existing
  trivia-capture path (extended in M1 only if not already
  present — see quickstart §3 task T-2).
- The CST is built lazily: with `emitCST_ = false`, no
  `CSTBuilder` exists and the parser's hot path is untouched.

**Spec mapping**:
- FR-018 (the directive-aware pre-pass is the only net-new
  parsing code; the CST is a side-effect of the existing parser).
- §2 of [research.md](./research.md).

---

## §4. Doc IR (`lib/Fmt/Doc.{h,cpp}`)

```cpp
class Doc {
public:
    enum class Kind { Text, Line, Nest, Group, Concat, Align, Comment };

    static std::shared_ptr<Doc> text(llvm::StringRef s);
    static std::shared_ptr<Doc> line();        // soft break
    static std::shared_ptr<Doc> hardline();    // mandatory break
    static std::shared_ptr<Doc> nest(int indent, std::shared_ptr<Doc> inner);
    static std::shared_ptr<Doc> group(std::shared_ptr<Doc> inner);
    static std::shared_ptr<Doc> concat(std::initializer_list<std::shared_ptr<Doc>>);
    static std::shared_ptr<Doc> align(std::shared_ptr<Doc> inner);
    static std::shared_ptr<Doc> comment(llvm::StringRef text, bool leading, bool trailing);
};
```

**Invariants**:
- `Doc` instances are immutable after construction (factory
  methods return `std::shared_ptr<const Doc>` but the public
  signature is non-const for Phase 1 simplicity; const-correctness
  is a Phase 2 polish — see quickstart §6).
- `Doc::group(d)` semantics: render `d` flat if it fits the
  remaining ribbon width; otherwise break at every `Line` inside.
- `Doc::comment` is the only Doc kind that carries trivia; all
  other kinds are pure layout primitives.

**Spec mapping**:
- §3 of [research.md](./research.md).
- Internal to `libNslFmt.a` — NOT in the public API contract.

---

## §5. Configuration record (`lib/Fmt/Config.{h,cpp}`)

```cpp
struct Configuration {
    enum class Indent { Spaces2, Spaces4, Tab };
    enum class BraceStyle { KAndR, Allman };
    enum class TrailingCommas { Preserve, Add, Remove };
    enum class CommentMode { All, LeadingOnly, None };

    Indent          indent                       = Indent::Spaces4;
    int             max_line_length              = 100;
    bool            spaces_around_binary_ops     = true;
    bool            spaces_inside_braces         = false;
    bool            align_struct_members         = true;
    bool            align_case_arrows            = true;
    BraceStyle      brace_style                  = BraceStyle::KAndR;
    TrailingCommas  trailing_commas              = TrailingCommas::Preserve;
    int             blank_lines_between_modules  = 2;
    CommentMode     preserve_comments            = CommentMode::All;
};
```

**Invariants**:
- Built-in defaults match the example `.nsl-fmt.toml` shown in
  [`docs/design/nsl_tooling_design.md`](../../docs/design/nsl_tooling_design.md)
  §5.1.
- Out-of-range / wrong-type values for known keys are rejected
  with a `basic::Diagnostic` and `Format::Result::Error` (FR-016).
- Unknown TOML keys produce a *warning* diagnostic but do NOT
  cause `Result::Error` (FR-015).
- `Configuration` is value-typed (no heap allocation); copies are
  cheap enough that every `format_buffer()` call gets its own.

**Spec mapping**:
- FR-013 / FR-014 / FR-015 / FR-016.
- The 10 fields here ↔ the 10 keys in design doc §5.1.

---

## §6. Format result (`lib/Fmt/Format.{h,cpp}`)

```cpp
struct FormatResult {
    enum class Status { Success, Refused, Error };

    Status                            status;
    std::string                       formattedText;     // valid only if status == Success
    std::vector<basic::Diagnostic>    diagnostics;       // always populated
};

FormatResult format_buffer(llvm::StringRef        sourceBuffer,
                           const Configuration   &config,
                           basic::FileID          fileID,
                           std::optional<LineRange> range = std::nullopt);
```

**Invariants**:
- `format_buffer()` is a pure function of `(sourceBuffer, config,
  fileID, range)`; no environment reads (Principle V byte-
  stability).
- `Status::Success` ⇒ `formattedText` is the canonical formatting;
  `formattedText.empty()` only if input was empty.
- `Status::Refused` ⇒ input is syntactically invalid (parse error
  or directive-splitter error); `diagnostics` carries the user-
  visible reason. NOT an internal error — it's the explicit
  refusal in FR-012.
- `Status::Error` ⇒ internal error (config malformed, range out
  of bounds, IO failure); `diagnostics` carries the cause.
- For every input that returns `Status::Success`,
  `format_buffer(format_buffer(s, c, f).formattedText, c, f)` MUST
  return `Status::Success` with the same `formattedText` (FR-008
  idempotence).

**Spec mapping**:
- FR-008 (idempotence) — invariant explicit above.
- FR-017 (library entry point) — signature here.
- FR-012 (refusal on parse error) — `Status::Refused` semantics.
- FR-018 (Principle II — Configuration is value-typed, no
  ownership of `libNSLFrontend.a` state leaks out).
- The CLI ↔ library parity gate (FR-022) reduces to: the CLI
  output equals `format_buffer().formattedText` for the same
  input + config.

---

## §7. Diff record (`lib/Fmt/Diff.{h,cpp}`)

```cpp
struct DiffHunk {
    int    oldStartLine;
    int    oldLineCount;
    int    newStartLine;
    int    newLineCount;
    std::vector<DiffLine> lines;
};

struct DiffLine {
    enum class Kind { Context, Removed, Added };
    Kind        kind;
    std::string text;
};

std::string emit_unified_diff(llvm::StringRef oldText,
                              llvm::StringRef newText,
                              llvm::StringRef oldName,
                              llvm::StringRef newName);
```

**Invariants**:
- `emit_unified_diff()` is a pure function (no environment reads).
- For identical inputs, returns the empty string.
- Output is a valid `diff -u` format: `--- oldName\n+++ newName\n
  @@ -<a>,<b> +<c>,<d> @@\n` plus `[ +-]<line>` rows.
- Determinism: the Myers-diff implementation chooses the
  lexicographically smallest hunk decomposition when ties exist
  (so output is reproducible across builds).

**Spec mapping**:
- FR-003 (`--check` prints unified diff per offending file).
- §5 of [research.md](./research.md).

---

## §8. Public API (`include/nsl/Fmt/Fmt.h`)

The Phase-1 freeze of the public surface is in
[`contracts/format-api.contract.md`](./contracts/format-api.contract.md)
§5. For the catalog:

| Symbol | Kind | Purpose |
|---|---|---|
| `nsl::fmt::Configuration` | struct | Configuration record (§5) |
| `nsl::fmt::FormatResult` | struct | Format-call return (§6) |
| `nsl::fmt::LineRange` | struct | `--range LINE:LINE` selector |
| `nsl::fmt::format_buffer` | free function | Top-level format entry point (§6) |
| `nsl::fmt::parse_config_file` | free function | TOML → Configuration |
| `nsl::fmt::discover_config` | free function | Upward `.nsl-fmt.toml` walk (FR-013) |
| `nsl::fmt::emit_unified_diff` | free function | `--check` diff (§7) |
| `nsl::fmt::default_configuration` | free function | Returns built-in defaults |
| `nsl::fmt::config_key_names` | free function | List of the 10 known keys (for unknown-key diagnostics) |
| `nsl::fmt::version_string` | free function | Version banner for `nsl-fmt --version` |

**Invariants**:
- 10 symbols total — count frozen by `format-api.contract.md` §5.
- All free functions are `noexcept(false)` and return values; no
  callbacks, no global state.
- `Configuration`, `FormatResult`, `LineRange` are value types
  (no `unique_ptr`, no opaque handles); cheap to copy.
- ABI stability is NOT promised at T2 (no `extern "C"`, no
  versioned symbols); T5 (LSP) and any other consumer rebuilds
  against the current `Fmt.h`.

**Spec mapping**:
- FR-017 / FR-019 (library-API surface).
- Principle II (single public umbrella header `Fmt.h`).

---

## §9. Test corpus taxonomy

| Corpus | Driver | Files | Spec mapping |
|---|---|---|---|
| `test/Fmt/rules/<rule>/` | lit + FileCheck | 6 (one per §5.3 rule) | FR-009 / FR-020 |
| `test/Fmt/cli/<surface>/` | lit + FileCheck | ~6 (stdin, in-place, check, range, multi-file, mutually-exclusive) | FR-001..FR-007, FR-003a |
| `test/Fmt/config/<aspect>/` | lit + FileCheck | 4 (discovery, unknown-key, invalid-value, explicit-config) | FR-013..FR-016 |
| `test/Fmt/directives/<class>/` | lit + FileCheck | 5 (include, ifdef, define, line, ident-splice) | FR-012a |
| `test/Fmt/edge/<edge>/` | lit + FileCheck | 5 (empty, CRLF, BOM, parse-error refusal, over-long line) | spec edge cases |
| `test/Fmt/idempotence/synthetic/` | lit + FileCheck | ~5 (constructed cases) | FR-008 / FR-021 |
| `test/Fmt/idempotence/audited/` | lit + FileCheck | 7 (one per audited project; UNSUPPORTED until M7) | FR-021 / SC-002 |
| `test_unit/Fmt/*_test.cc` | GoogleTest | 5 (parity, splitter, config, doc, diff) | FR-022 + invariants |

Total: ~38 distinct test files at T2 acceptance (with 7 of those
in the "active when M7 lands" pool).

**Output gate**: ✅ Entity catalog complete; ready for contract
authoring.
