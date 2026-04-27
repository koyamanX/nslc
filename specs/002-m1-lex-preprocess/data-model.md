<!-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception -->

# Data Model: M1 — Lex + Preprocess (with Diagnostic Plumbing)

**Branch**: `002-m1-lex-preprocess` | **Date**: 2026-04-27
**Plan**: [plan.md](./plan.md)

The Key Entities section of [spec.md](./spec.md) lists the
user-facing entities. This file expresses them structurally — fields,
invariants, lifecycle — so the Phase 2 tasks plan and the
implementation can verify that the design honors every spec FR.

The structure-of-truth for spec → design coupling is the cross-
reference table in `docs/CLAUDE.md` §8; design rows for these
entities live in `docs/design/nsl_compiler_design.md` §12 (lines
1163–1195) for `DiagnosticEngine` and §3 (lines 132–148) for the
layer skeleton. The design doc does not yet contain a full
expansion of `SourceManager` / `Token` / `Lexer` / `Preprocessor`;
this file fills that gap as the M1 implementation reference.

## Entity 1 — `SourceLocation` (header: `nsl/Basic/SourceLocation.h`)

**Purpose**: smallest unit of source attribution. Every Token,
diagnostic, and (later) AST node carries one or two of these.

**Representation**: an opaque 32-bit handle (pointer-stable, value-
type, comparable, printable).

```cpp
class SourceLocation {
  uint32_t bits_;       // 24 bits offset within Buffer | 8 bits FileID index
public:
  SourceLocation() = default;                           // invalid sentinel
  static SourceLocation make(FileID fid, uint32_t off); // pre: off < 16 MiB
  FileID    file()    const noexcept;
  uint32_t  offset()  const noexcept;
  bool      isValid() const noexcept;
  bool operator==(SourceLocation other) const noexcept;
  bool operator< (SourceLocation other) const noexcept; // total order: (file, offset)
};
```

**Invariants**:
- A default-constructed `SourceLocation` is invalid (`bits_ == 0`,
  treated as sentinel by `isValid()`).
- `offset()` is always within the corresponding `Buffer.size()`
  (asserted via `SourceManager::checkLoc`).
- `make()` rejects `off >= 16 MiB` with a hard fatal — at M1, no NSL
  source file plausibly exceeds that limit; if a future user hits
  it, the right answer is to widen the field, not to silently
  truncate.

**Lifecycle**: created by `SourceManager` (the only blessed source);
copied freely as a value type; never reused for a different file.

## Entity 2 — `SourceRange` (header: `nsl/Basic/SourceLocation.h`)

**Purpose**: the source-attribution carrier for a *span* of source —
a Token, an expression, an AST node.

**Representation**:

```cpp
class SourceRange {
  SourceLocation begin_;
  SourceLocation end_;       // half-open: [begin, end)
public:
  SourceRange() = default;                                          // invalid
  SourceRange(SourceLocation b, SourceLocation e);                  // pre: b <= e, same file
  SourceLocation begin()  const noexcept;
  SourceLocation end()    const noexcept;
  uint32_t       length() const noexcept;                           // end.offset - begin.offset
  bool           contains(SourceLocation loc) const noexcept;
  bool           isValid() const noexcept;                          // both endpoints valid + same file
};
```

**Invariants**:
- `begin_.file() == end_.file()` (asserted in constructor).
- `begin_ <= end_` (asserted).
- A `SourceRange` is invalid iff either endpoint is invalid.

**Lifecycle**: identical to `SourceLocation`.

## Entity 3 — `FileID` (header: `nsl/Basic/SourceLocation.h`)

**Purpose**: identifier of a buffer registered with the
`SourceManager`. Encoded into `SourceLocation`'s top 8 bits.

**Representation**: a strong-typed wrapper over `uint8_t`.

```cpp
class FileID {
  uint8_t id_;
  explicit FileID(uint8_t id) : id_(id) {}
  friend class SourceManager;
public:
  FileID() : id_(0) {}              // 0 = invalid sentinel
  uint8_t   raw()     const noexcept { return id_; }
  bool      isValid() const noexcept { return id_ != 0; }
  bool operator==(FileID other) const noexcept;
};
```

**Invariants**:
- `id_ == 0` is the invalid sentinel.
- `id_ ∈ [1, 255]` are valid IDs allocated by `SourceManager` in
  insertion order.

**Lifecycle**: allocated once per file load; never reused.

## Entity 4 — `Buffer` (private to `nsl/Basic/SourceManager.h` impl)

**Purpose**: in-memory representation of a loaded source file.

**Representation** (private):

```cpp
struct Buffer {
  std::string         path;          // canonical absolute path (post-realpath)
  std::vector<char>   bytes;         // file contents, NUL-terminated for safety
  // built lazily on first (file, line, col) query:
  std::vector<uint32_t> line_offsets; // [0]==0; [i]==offset of line i+1's first char
  // populated by Preprocessor as it sees `#line` directives:
  std::vector<LineDirective> line_overrides;
};

struct LineDirective {
  uint32_t  origin_offset;       // physical offset where the override took effect
  uint32_t  virtual_line;        // first line number AFTER the directive
  std::string virtual_path;      // empty == reuse current path
};
```

**Invariants**:
- `bytes` ends with a NUL byte (allows trailing-end scan without
  bounds checks).
- `line_offsets` is sorted ascending (built lazily; rebuilt on
  invalidation never — files are immutable post-load).
- `line_overrides` is sorted by `origin_offset`; binary search at
  query time selects the active override.

**Lifecycle**: created by `SourceManager::loadFile()`; never
modified after load (line-offset table and overrides are
*augmentations*, not edits to `bytes`).

## Entity 5 — `SourceManager` (header: `nsl/Basic/SourceManager.h`)

**Purpose**: owns all `Buffer`s; resolves location queries; honors
`#line` adjustments.

**Public API** (excerpt):

```cpp
class SourceManager {
public:
  SourceManager();
  ~SourceManager();

  // File loading.
  llvm::ErrorOr<FileID> loadFile(llvm::StringRef path);
  FileID                addBufferInMemory(std::string path,
                                          std::vector<char> bytes);
  llvm::StringRef       getBuffer(FileID f) const;
  llvm::StringRef       getPath  (FileID f) const;

  // Location queries (physical).
  std::pair<uint32_t, uint32_t>  getLineCol(SourceLocation loc) const;  // (1-based line, 1-based col)
  llvm::StringRef                getLine   (SourceLocation loc) const;  // raw line text

  // Location queries (virtual — post-#line).
  struct VirtualLoc { llvm::StringRef path; uint32_t line; uint32_t col; };
  VirtualLoc  resolveVirtual(SourceLocation loc) const;

  // #line directive registration (called by Preprocessor).
  void        addLineDirective(SourceLocation at,
                               uint32_t virtual_line,
                               llvm::StringRef virtual_path);

  // Diagnostic include-stack support (used by DiagnosticEngine).
  void        pushIncludeFrame(SourceLocation include_directive_loc, FileID included);
  void        popIncludeFrame();
  std::vector<SourceLocation>  getIncludeStackFor(FileID f) const;
};
```

**Invariants**:
- `loadFile()` is idempotent — loading the same canonical path twice
  returns the same `FileID`. (Required for correct cycle detection
  in the include-stack.)
- `getLineCol()` lazily builds the line-offset table on first call
  per file; subsequent calls are O(log lines).
- `resolveVirtual()` returns the *post-`#line`* coordinates if a
  matching `LineDirective` exists at or before `loc.offset()`;
  otherwise returns the physical coordinates.
- `addLineDirective()` requires `at.offset()` strictly greater than
  the most recent override for that file (no out-of-order
  insertion).

**Lifecycle**: one `SourceManager` per `Compilation`; lives for the
whole compile; never partially destroyed.

## Entity 6 — `DiagnosticEngine` (header: `nsl/Basic/Diagnostic.h`)

**Purpose**: the sole emitter of diagnostics for any layer in
`libNSLFrontend.a`.

**Public API** (excerpt):

```cpp
enum class Severity : uint8_t { Note, Warning, Error };

struct FixItHint {
  SourceRange range;
  std::string replacement;
};

class DiagnosticEngine {
public:
  DiagnosticEngine(SourceManager& sm);

  // Emit a diagnostic. Returns a builder for fixit hints / notes.
  class Builder;
  Builder report(Severity s, SourceLocation loc, std::string msg);

  // Output formatting.
  enum class Format : uint8_t { Text, JSON };
  void renderAll(llvm::raw_ostream& os, Format fmt) const;

  // Query.
  size_t  numErrors()        const noexcept;
  size_t  numWarnings()      const noexcept;
  bool    hasError()         const noexcept { return numErrors() > 0; }
  void    clear()            noexcept;

  // For testing.
  llvm::ArrayRef<Diagnostic>  diagnostics() const noexcept;
};

struct Diagnostic {
  Severity                  severity;
  SourceLocation            loc;
  std::string               message;
  std::vector<FixItHint>    fixits;
  std::vector<Diagnostic>   notes;       // include-stack notes appended here
};
```

**Invariants**:
- `report()` is single-threaded (M1 has no concurrent diagnostic
  paths).
- `renderAll(Text)` emits `<path>:<line>:<col>: <severity>: <message>`
  per FR-025, sorted by `(loc, severity)` for determinism (research
  §4).
- `renderAll(JSON)` emits NDJSON per research §9.
- `clear()` resets the buffer; `numErrors()` returns 0 thereafter.
- `Diagnostic.notes` is populated by `Builder::addIncludedFromNote()`
  using the `SourceManager` include-stack at emit time.

**Lifecycle**: one `DiagnosticEngine` per compile; outlives all
buffers.

## Entity 7 — `TokenKind` (header: `nsl/Lex/Token.h`)

**Purpose**: classify a `Token` for downstream consumers.

**Representation**: an `enum class` populated from two X-macro `.def`
files (research §6, §7):

```cpp
enum class TokenKind : uint16_t {
  // ----- Sentinels -----
  tk_unknown,
  tk_eof,

  // ----- Whitespace-producing (filtered out before consumer sees them) -----
  // (none enumerated — comments + whitespace are skipped at scan time)

  // ----- Identifiers + literals -----
  tk_identifier,
  tk_string_lit,
  tk_decimal_lit,
  tk_hex_lit,
  tk_binary_lit,
  tk_octal_lit,

  // ----- _-prefix system names (per N11) -----
  tk_system_task,
  tk_system_function,
  tk_unused_underscore,

  // ----- Reserved keywords (one per entry in lang.ebnf §15) -----
  // Generated from include/nsl/Lex/KeywordSet.def
#define KEYWORD(name, spelling) tk_##name,
#include "nsl/Lex/KeywordSet.def"
#undef KEYWORD

  // ----- Punctuation / operators -----
  tk_lparen, tk_rparen, tk_lbrace, tk_rbrace, tk_lbracket, tk_rbracket,
  tk_comma, tk_semicolon, tk_colon, tk_dot,
  tk_assign, tk_assign_seq,             // = vs := (S3)
  tk_plus, tk_minus, tk_star, tk_slash, tk_percent,
  tk_amp, tk_pipe, tk_caret, tk_tilde,  // bitwise (N2 disambiguates reduction-vs-bitwise upstream)
  tk_logical_and, tk_logical_or, tk_logical_not,
  tk_equal, tk_not_equal,
  tk_less, tk_less_equal, tk_greater, tk_greater_equal,
  tk_shift_left, tk_shift_right,
  tk_question, tk_at,
  tk_hash_sign_extend,                  // # in expression position (N5)
  tk_apostrophe_zero_extend,            // ' (zero-extend)
  tk_dot_lbrace,                        // .{ (N3)

  // ----- Preprocessor seam markers (P13) -----
  tk_line_directive,                     // #line N "file" passed through to M2 parser

  tk_count
};
```

The numeric subkind (Z / X / U digits) is carried as a `Token::flags`
attribute (research §7), not as a `TokenKind` enumerator.

**Invariants**:
- `tk_count` is the sole non-named enumerator and serves as the
  array-size sentinel for tables that key by kind.
- Every keyword in `KeywordSet.def` produces exactly one `tk_<name>`
  enumerator; missing entries are caught by a static-assert in
  `lib/Lex/KeywordSet.cpp` over the `KEYWORD(...)` count.

## Entity 8 — `Token` (header: `nsl/Lex/Token.h`)

**Purpose**: the unit emitted by `Lexer::next()`.

**Representation**:

```cpp
class Token {
public:
  TokenKind        kind()    const noexcept;
  SourceRange      range()   const noexcept;
  llvm::StringRef  spelling() const noexcept;     // view into Buffer (NOT owned)
  uint16_t         flags()   const noexcept;       // numeric subkind (Z|X|U), prefix kind, etc.
  enum NumericFlag : uint16_t {
    NF_Plain  = 0,
    NF_HasZ   = 1u << 0,
    NF_HasX   = 1u << 1,
    NF_HasU   = 1u << 2,
  };
private:
  TokenKind   kind_;
  uint16_t    flags_;
  SourceRange range_;
  llvm::StringRef spelling_;
};
```

**Invariants**:
- `range_.length() == spelling_.size()` (the spelling is the
  literal span in the buffer).
- `spelling_` aliases the originating `Buffer` and is invalidated
  if the `SourceManager` ever moves a buffer (it doesn't —
  `Buffer.bytes` is stable post-load).
- `kind_ == tk_eof` ↔ `range_.length() == 0` and `spelling_.empty()`.
- For numeric tokens (`tk_*_lit`), `flags_ & (NF_HasZ | NF_HasX |
  NF_HasU)` reflects the digits actually present.

**Lifecycle**: produced by `Lexer::next()`; copied by value (cheap —
3 pointers + 32-bit ints). Lives only as long as the originating
`Buffer` does (which is the entire compile).

## Entity 9 — `Lexer` (header: `nsl/Lex/Lexer.h`)

**Purpose**: stateful pull-model scanner over a single `Buffer`.

**Public API** (excerpt):

```cpp
class Lexer {
public:
  Lexer(SourceManager& sm, FileID fid, DiagnosticEngine& diag);
  Token   next();         // pulls one token; emits tk_eof at end
  Token   peek(int n=0);  // lookahead without consuming (cached)
  bool    atEOF() const noexcept;

private:
  SourceManager&     sm_;
  DiagnosticEngine&  diag_;
  FileID             fid_;
  uint32_t           cur_;          // current offset into Buffer.bytes
  llvm::StringRef    buf_;          // == sm_.getBuffer(fid_)
  llvm::StringMap<TokenKind> keyword_set_;  // built from KeywordSet.def at construction
  std::deque<Token>  peek_cache_;
  bool               at_line_start_; // for N5 disambiguation
};
```

**State transitions**: `at_line_start_` toggles on every `\n` consumed
and on every `#line` directive consumed (P13 sets it to `true` for
the line *after* the directive). All other state is the byte
cursor.

**Invariants**:
- `cur_ <= buf_.size()`; equality means EOF.
- `at_line_start_` correctly identifies whether the next non-
  whitespace `#` should be classified as `tk_line_directive` (start
  of line) or `tk_hash_sign_extend` (mid-line) — N5 disambiguation.
- `peek(n)` is consistent with `n` consecutive `next()` calls.

## Entity 10 — `MacroDef` (private to `nsl/Preprocess/Preprocessor.cpp`)

**Purpose**: storage record for a `#define`'d macro.

**Representation** (private):

```cpp
struct MacroDef {
  llvm::StringRef        name;            // view into Buffer of defining file
  std::vector<RawToken>  body;            // unexpanded body per P10
  SourceRange            defining_loc;    // for "redefined here" notes
};

struct RawToken {
  // Smaller than Token: the preprocessor doesn't classify keywords
  // until macro-expansion is complete and the resulting text is
  // re-tokenized.
  llvm::StringRef text;
  SourceRange     range;
  enum Kind : uint8_t { Text, PercentMacroRef, HelperCall };
  Kind            kind;
};
```

**Invariants**:
- `body` is stored unexpanded. Expansion order at use sites follows
  P10: %IDENT% splice → helper eval → operator reduction.
- `name` is unique within the `MacroTable`; redefinition replaces
  the entry and emits a `note: previous definition was here` if the
  redefinition differs textually.

## Entity 11 — `MacroTable` (private to `nsl/Preprocess/`)

**Purpose**: insertion-ordered name → `MacroDef` map.

**Representation**: `llvm::MapVector<llvm::StringRef, MacroDef>`
(research §4).

**Invariants**:
- Iteration order is insertion order — never hash-derived.
- `lookup(name)` is O(1) average via the internal `DenseMap`.
- `undef(name)` removes the entry; the stable iteration of
  surviving entries is preserved.
- Predefined macros (from `-D` flags) are inserted in
  command-line order *before* any source-defined macro; this gives
  `-D` macros priority for "first definition wins" determinism.

## Entity 12 — `IncludeFrame` (private to `nsl/Preprocess/`)

**Purpose**: stack frame for the include-recursion model.

**Representation** (private):

```cpp
struct IncludeFrame {
  FileID         fid;
  uint32_t       cursor;                         // offset into bytes
  SourceLocation include_directive_loc;          // where this frame was pushed
};
```

The `Preprocessor` holds a `std::vector<IncludeFrame>` (used as a
stack — back is the active frame). Cycle detection is depth-bounded
at 256 (FR-022).

**Invariants**:
- `frames_.size() <= 256`; exceeding the limit raises an "include
  cycle detected: <path-trace>" error.
- The **bottom** frame is the original `nslc` input file; popping
  it ends the preprocess pass.

## Entity 13 — `PPValue` (private to `nsl/Preprocess/`)

**Purpose**: result type of the helper evaluator and `#if`
expression evaluator.

**Representation**:

```cpp
class PPValue {
  std::variant<int64_t, long double> v_;
public:
  bool       isInt()    const noexcept;
  bool       isReal()   const noexcept;
  int64_t    toInt()    const;       // truncate-toward-zero on real (= _int semantics)
  long double toReal()  const;       // widen integer
  bool       isTruthy() const noexcept;  // P4: non-zero on either kind
  std::string render()  const;        // for substitution into output stream
};
```

**Invariants**: per research §5 (numeric model and coercion).

## Entity 14 — `IncludeSearchPath` (header: `nsl/Preprocess/Preprocessor.h`)

**Purpose**: ordered list of directories searched for `#include`.

**Representation**:

```cpp
class IncludeSearchPath {
public:
  IncludeSearchPath();
  void  appendQuotePath(llvm::StringRef dir);    // -I dir, quote-form (P8)
  void  appendAnglePath(llvm::StringRef dir);    // NSL_INCLUDE entries, angle-form (P8)

  llvm::ErrorOr<std::string>
        findQuote(llvm::StringRef filename, llvm::StringRef including_dir) const;
  llvm::ErrorOr<std::string>
        findAngle(llvm::StringRef filename) const;
};
```

**Invariants**:
- Quote-form search order: (1) `including_dir`, (2) every
  `appendQuotePath`'d entry in registration order. (Per pp.ebnf P8.)
- Angle-form search order: every `appendAnglePath`'d entry in
  registration order. The `NSL_INCLUDE` env var is parsed once at
  `Preprocessor` construction time and feeds `appendAnglePath` in
  PATH order.
- Both `find*` are pure functions of `(filename, including_dir,
  appended path lists)` — no environment-derived behavior beyond
  the one-time `NSL_INCLUDE` read.

---

## State transition diagrams

### `Preprocessor::run()` lifecycle

```
[Construct]
    |
    | open(input_file) → loadFile → push IncludeFrame[0]
    v
[Reading lines]
    |
    +-- directive line ──► [DirectiveParser::parse]
    |                          |
    |                          +-- #include ──► resolve via IncludeSearchPath ──► push frame ──► emit `#line 1 "f"`
    |                          +-- #define / #undef ──► update MacroTable
    |                          +-- #if* / #else / #endif ──► update conditional stack (P9)
    |                          +-- #line ──► sm.addLineDirective + emit canonical `#line` (P13)
    |
    +-- passthrough line ──► IdentSplicer (resolve %IDENT% per P3) ──► emit text tokens
    |
    | EOF on top frame → pop ──► emit `#line N "outer"` re-establishment marker
    |
    | bottom frame popped → tk_eof
    v
[Done]
```

### `Lexer::next()` decision flow

```
peek(0):
  if at_line_start_ and current is '#':
    classify per N5: try `#line` form → tk_line_directive
                    else                tk_hash_sign_extend
  else if isIdentStart(current):
    scan identifier; classify via keyword_set_, tk_system_*, or tk_identifier
  else if isDigit(current) or current is '0' followed by [bBoOxX]:
    scan number; set NF_HasZ/X/U flags as digits encountered
  else if current is '"':
    scan string literal; raise diag on unterminated
  else if current is whitespace:
    skip; recurse
  else if current is '/' and next is '/' or '*':
    skip comment; recurse
  else:
    single-char punctuation table lookup; tk_unknown if no match
```

---

## Validation checklist for the design

Per spec FR roll-up:
- **FR-001/002/003** (three libraries): entities 1–14 partition cleanly across nsl-basic / nsl-preprocess / nsl-lex per the headers cited.
- **FR-004** (add_nsl_library declaration): plan.md "Source Code" section confirms each library has a CMakeLists.txt under `lib/<Layer>/` calling `add_nsl_library(...)`.
- **FR-005..012** (lexer behavior): entity 7 (TokenKind) + entity 8 (Token) + entity 9 (Lexer) cover every requirement.
- **FR-013..023** (preprocessor behavior): entity 10–14 + the `Preprocessor::run()` flow cover every requirement.
- **FR-024..028** (diagnostic plumbing): entity 5 (SourceManager) + entity 6 (DiagnosticEngine) cover every requirement; the include-stack note machinery is `SourceManager::pushIncludeFrame` + `DiagnosticEngine::Builder::addIncludedFromNote`.
- **FR-029/030** (driver flag): plan.md "Source Code" section places `lib/Driver/EmitTokens.cpp` and the `tools/nslc/main.cpp` switch case.
- **FR-031..037** (test gates): research §8 specifies the corpus; this file does not duplicate.
- **FR-038/039** (determinism): research §4 specifies; entities 11 (MacroTable as `MapVector`) and 6 (DiagnosticEngine sort-on-render) implement.

If any cell of the FR roll-up has no corresponding entity here, that
is a design gap to fix before tasks.md is generated.
