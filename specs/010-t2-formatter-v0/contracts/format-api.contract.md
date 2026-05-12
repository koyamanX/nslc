<!-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception -->

# Contract: `libNslFmt.a` public API (T2 freeze)

**Branch**: `010-t2-formatter-v0` | **Date**: 2026-05-04
**Plan**: [../plan.md](../plan.md) | **Spec**: [../spec.md](../spec.md)
**Data model**: [../data-model.md §8](../data-model.md)

This contract freezes the public surface of `libNslFmt.a` as
exposed through the single umbrella header
`include/nsl/Fmt/Fmt.h`. T5 (LSP integration) and any other
consumer rebuilds against this header. ABI is NOT promised at
T2; source compatibility is.

---

## §1. Header layout

```text
include/nsl/Fmt/
└── Fmt.h            // sole public header (Principle II)
```

**No other public header exists in `include/nsl/Fmt/`.** The
`nsl-fmt` library is NOT one of the named header-layout
exceptions (`nsl-ast`, `nsl-sema`) under Principle II. Internal
headers stay in `lib/Fmt/`.

---

## §2. Namespace

All public symbols live under `nsl::fmt::`. Internal helpers
under `lib/Fmt/` MAY use `nsl::fmt::detail::` but MUST NOT
appear in `Fmt.h`.

---

## §3. Symbol catalog (frozen — exactly 10 symbols)

```cpp
namespace nsl::fmt {

// --- Configuration ---
struct Configuration {
    enum class Indent          { Spaces2, Spaces4, Tab };
    enum class BraceStyle      { KAndR, Allman };
    enum class TrailingCommas  { Preserve, Add, Remove };
    enum class CommentMode     { All, LeadingOnly, None };

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

// --- Range selector ---
struct LineRange {
    int firstLine;   // 1-indexed, inclusive
    int lastLine;    // 1-indexed, inclusive; >= firstLine
};

// --- Result ---
struct FormatResult {
    enum class Status { Success, Refused, Error };
    Status                            status;
    std::string                       formattedText;     // valid iff status == Success
    std::vector<basic::Diagnostic>    diagnostics;       // always populated; may be empty on Success
};

// --- Free functions (frozen — exactly 7) ---

FormatResult format_buffer(llvm::StringRef        sourceBuffer,
                           const Configuration   &config,
                           basic::FileID          fileID,
                           std::optional<LineRange> range = std::nullopt);

FormatResult parse_config_file(llvm::StringRef tomlBuffer,
                               basic::FileID   fileID,
                               Configuration *out);

std::optional<std::string> discover_config(llvm::StringRef startDir);

std::string emit_unified_diff(llvm::StringRef oldText,
                              llvm::StringRef newText,
                              llvm::StringRef oldName,
                              llvm::StringRef newName);

Configuration default_configuration() noexcept;

llvm::ArrayRef<llvm::StringRef> config_key_names() noexcept;

llvm::StringRef version_string() noexcept;

} // namespace nsl::fmt
```

**Symbol count audit**: 3 types (`Configuration`, `LineRange`,
`FormatResult`) + 7 free functions = **10 symbols**. The CI
script `audit_fmt_api.sh` (added at quickstart §10) greps
`Fmt.h` for top-level declarations and asserts the count
equals 10.

---

## §4. Function contracts

### `format_buffer`

- **Pre**: `sourceBuffer` may be empty; `fileID` must be valid
  for diagnostic source-range mapping; `range`, if set, must
  satisfy `1 <= firstLine <= lastLine <= LineCount(sourceBuffer)`.
- **Post (Success)**: `formattedText` is the canonical
  formatting under `config`; `diagnostics` may carry
  warnings (e.g., over-long line). **When `formattedText` is
  non-empty, it ends with exactly one `\n`** (clarified
  Session 2026-05-05 — Q3 trailing-newline policy). Idempotence:
  `format_buffer(format_buffer(s, c, f).formattedText, c, f)`
  returns `Success` with the same `formattedText`.
- **Post (Refused)**: `formattedText` is empty;
  `diagnostics` carries at least one `Error`-level
  diagnostic naming the parse-error location. Caller MUST
  NOT consume `formattedText`. **The refusal is atomic** —
  if ANY of the input's NSLFragment slices fails to lex+parse,
  the whole `format_buffer` call refuses; partial output is
  never emitted (clarified Session 2026-05-05 — Q1 strict
  refusal). Tolerated pre-parse byte sequences are limited to
  those named in FR-012a (directive lines + `%IDENT%` splices);
  any other byte sequence the lexer cannot tokenise (BOM,
  vendor pragmas, etc.) triggers refusal.
- **Post (Error)**: `formattedText` is empty; `diagnostics`
  carries the internal failure cause (range-out-of-bounds,
  config malformed, IO failure). Caller MUST NOT consume
  `formattedText`.
- **Determinism**: pure function of inputs (no environment
  reads). Per Principle V: same inputs ⇒ same outputs across
  builds, hosts, and processes.

### `parse_config_file`

- **Pre**: `tomlBuffer` is the full content of the TOML file;
  `fileID` provides the `SourceRange` reference for diagnostics.
  `out` is a non-null pointer to a `Configuration` instance the
  function will overwrite on success.
- **Signature evolution (Session 2026-05-12)**: the third
  parameter `Configuration *out` was added during Phase 6
  implementation (T103) — the original two-param contract
  proposed returning the parsed config via a `ConfigParsed`
  diagnostic, but the implementation chose the simpler explicit
  out-pointer form. `out` MUST be non-null (the implementation
  asserts).
- **Post (Success)**: `*out` is populated with the parsed
  configuration; `formattedText` is empty; `status` is
  `Success`.
- **Post (Error)**: `*out` is left unchanged; `diagnostics`
  carries the TOML parse error or out-of-range value
  diagnostic; `status` is `Error`.

### `discover_config`

- **Pre**: `startDir` is a directory path; behaviour is
  undefined for non-existent paths (caller's responsibility).
- **Post**: returns the path to the first `.nsl-fmt.toml`
  found by walking upward from `startDir`; returns
  `std::nullopt` if none found before reaching the filesystem
  root.
- **Determinism**: depends on filesystem state, not
  environment variables — bound to the real file tree at call
  time. This is the only function in the API that is NOT a
  pure function; callers wanting determinism (CI fixtures)
  pass `--config <explicit path>` to bypass discovery.

### `emit_unified_diff`

- **Pre**: any two `StringRef`s.
- **Post**: empty string if `oldText == newText`;
  otherwise a valid `diff -u` output with the named files
  in the headers. Determinism: the Myers-diff implementation
  picks the lexicographically smallest hunk decomposition
  for ties.

### `default_configuration`

- Returns the `Configuration` value with every field at its
  built-in default (matching `Configuration` struct
  initialisers). Constexpr-evaluated; no allocation.

### `config_key_names`

- Returns a static `ArrayRef<StringRef>` listing the 10
  Configuration keys in declaration order. Used by the
  unknown-key warning to suggest "did you mean…?" matches.

### `version_string`

- Returns a static `StringRef` of the form
  `nsl-fmt version <NSL_PROJECT_VERSION> (LLVM
  <LLVM_PROJECT_VERSION>)`. Sourced from CMake-defined
  preprocessor symbols.

---

## §5. Public-symbol audit (frozen at 10)

| # | Symbol | Kind |
|---|---|---|
| 1 | `nsl::fmt::Configuration` | struct |
| 2 | `nsl::fmt::LineRange` | struct |
| 3 | `nsl::fmt::FormatResult` | struct |
| 4 | `nsl::fmt::format_buffer` | free function |
| 5 | `nsl::fmt::parse_config_file` | free function |
| 6 | `nsl::fmt::discover_config` | free function |
| 7 | `nsl::fmt::emit_unified_diff` | free function |
| 8 | `nsl::fmt::default_configuration` | free function |
| 9 | `nsl::fmt::config_key_names` | free function |
| 10 | `nsl::fmt::version_string` | free function |

**Add a symbol = contract change.** The CI grep
`scripts/audit_fmt_api.sh` enforces the count.

---

## §6. Determinism axes (Principle V)

Every API function except `discover_config` MUST be a pure
function of its inputs. The CI grep `scripts/audit_determinism.sh`
extends to `lib/Fmt/` and forbids:

- `std::unordered_map`, `std::unordered_set` (pointer-derived
  iteration order).
- Pointer-derived ordering (`std::less<T*>` on heap addresses).
- Time sources (`std::chrono`, `time(nullptr)`, `getenv`).
- Hostname / username / CWD / locale reads.

`discover_config` is the explicit exception (it reads the
filesystem); CI fixtures bypass it via `--config <path>`.

---

## §7. Error-flow diagram

```
              format_buffer()
                    │
                    ▼
       ┌────────────┴────────────┐
       │                         │
   directive                  config
   splitter                   validate
       │                         │
   parse error?              malformed?
       │                         │
       ▼                         ▼
  Status::Refused           Status::Error
   diagnostics:              diagnostics:
   parse-error               TOML/range error
                    │
                    ▼
              all good ──> CST mode parse
                    │
                    ▼
              LayoutPlanner ──> Doc IR
                    │
                    ▼
              LayoutRenderer ──> string
                    │
                    ▼
              Status::Success
              formattedText: <result>
              diagnostics: warnings only
```

---

## Spec cross-reference

| Spec FR | This contract section |
|---|---|
| FR-008 | §4 (idempotence post-condition) |
| FR-014–FR-016 | §3 Configuration / §4 parse_config_file |
| FR-017 | §3 (signature) |
| FR-018 | §1, §2 (Principle II) |
| FR-019 | §3 (range parameter on `format_buffer`) |
| FR-022 | The CLI ↔ library parity test invokes `format_buffer` on the same input the CLI uses |
| Principle V | §6 |
| Principle IV | §3 (`FormatResult::diagnostics` carries `basic::Diagnostic`) |
