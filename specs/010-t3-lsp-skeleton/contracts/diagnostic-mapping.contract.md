<!-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception -->

# Contract: Diagnostic Mapping (`nsl::Diagnostic` → LSP `Diagnostic`)

**Branch**: `010-t3-lsp-skeleton` | **Date**: 2026-05-05
**Anchors**: spec FR-010, FR-011, FR-012, FR-013; SC-002, SC-003

This contract freezes how `nsl-lsp` translates `libNSLFrontend.a`
diagnostic output into LSP `Diagnostic` objects on the wire. The
seam is implemented as a free function pair in
`lib/LSP/DiagnosticMapper.{h,cpp}`:

```cpp
namespace nsl::lsp {
llvm::json::Value toLspDiagnostic(const nsl::Diagnostic& d,
                                   const nsl::SourceManager& sm);
llvm::json::Value toLspDiagnosticArray(llvm::ArrayRef<nsl::Diagnostic> diags,
                                        const nsl::SourceManager& sm);
}
```

---

## §1 `code` field — diagnostic ID lookup

The LSP `Diagnostic.code` field is a stable string ID derived from
the diagnostic's origin. The mapping is implemented as a small
lookup table in `lib/LSP/DiagnosticMapper.cpp`, populated by:

| Diagnostic source                  | `code` form    | Source for the lookup                                     |
| ---------------------------------- | -------------- | --------------------------------------------------------- |
| Sema constraint diagnostics (`Sn`) | `S01` … `S29`  | `specs/006-m3-sema/contracts/diagnostic-string.contract.md` (existing M3 contract — message-prefix → `Sn` mapping) |
| Parser disambiguation notes (`Nn`) | `N01` … `N14`  | Hand-coded; one row per parser-note diagnostic that exists in the M2 parser today |
| Preprocessor notes (`Pn`)          | `P01` … `P13`  | Hand-coded per `pp.ebnf` notes |
| Other parser errors (no `Nn` ID)   | `parse`        | Catch-all for parser-emitted diagnostics not tied to a specific `Nn` |
| Other preprocessor errors          | `preprocess`   | Catch-all for preprocessor-emitted diagnostics |

The lookup is keyed on the **message prefix** the existing
`DiagnosticEngine` emits — e.g., a Sema diagnostic for `S01`
already begins with `error: identifier contains '__'` (or whatever
M3's diagnostic-string contract froze for `S01`). The mapper reads
the M3 contract's frozen prefix list and matches.

**Test**: `diagnostics_test::CodeMapping_S01` … `CodeMapping_S29`
each open a fixture triggering the corresponding `Sn`, send
`didOpen`, and assert the published `code` field equals `"S01"` …
`"S29"` respectively. Covers SC-002 (100% of diagnostic-emitting
`Sn` round-trip with the correct `code`).

---

## §2 `severity` field — `nsl::Severity` → LSP severity

```
nsl::Severity::Error    → 1   (LSP DiagnosticSeverity.Error)
nsl::Severity::Warning  → 2   (LSP DiagnosticSeverity.Warning)
nsl::Severity::Note     → 3   (LSP DiagnosticSeverity.Information)
```

**Rationale for `Note → Information(3)`**: LSP has no separate "Note"
severity. The closest match is `Information`, used by clangd and
rust-analyzer for the same purpose (informational detail attached
to a primary diagnostic). LSP's `Hint(4)` is reserved for
greyed-out / fade-out cues (unused symbols, etc.) and is not a
correct fit for `nsl::Diagnostic` notes.

---

## §3 `range` field — `nsl::SourceLocation` → LSP `Range`

`nsl::Diagnostic.loc` is a `SourceLocation` — a `(file, byte-offset)`
pair plus a length (or implicit "single-character"). Conversion:

1. Use `SourceManager` to resolve `loc` to `(filename, line,
   col-byte)`. Both `line` and `col-byte` are **one-based** in
   `nsl::SourceManager`'s convention.
2. Subtract 1 from each to get LSP's zero-based `(line, character)`.
3. Apply `byteOffsetToLspPosition` (R6) to convert the byte column
   to a UTF-16 code-unit column.
4. The `range.end` position is computed by advancing one source
   token (or the diagnostic's stored length, whichever is more
   precise) from the start.

If `Diagnostic.loc` resolves to a file inside an `#include`-pulled
region, `SourceManager` exposes the include stack via
`sourceManager().includeStackAt(loc)` (already implemented for
M3's `--diagnostic-format=json`). The mapper uses the **innermost
file's location** for the LSP `range`; the include chain
materializes as `relatedInformation` per §5 below.

**Determinism (SC-003)**: `byteOffsetToLspPosition` is pure (no
side effects, no global state); given the same `(line, byteOffset)`
input it produces the same `(line, character)` output. No
nondeterminism crosses the seam.

---

## §4 `source` field

The mapper consults a small per-diagnostic origin tag set on
`Diagnostic` at emission time. For T3, three values:

```
"nsl-sema"        // emitted by Sema (lib/Sema/)
"nsl-parse"       // emitted by Parser (lib/Parse/)
"nsl-preprocess"  // emitted by Preprocessor (lib/Preprocess/)
```

The origin tag is determined by the call site that originally
called `DiagnosticEngine.report(...)`. This is achievable without
modifying the existing `Diagnostic` struct: the mapper inspects
`Diagnostic.message`'s prefix to disambiguate (since M3, parser,
and preprocessor diagnostics use distinct prefix conventions —
e.g., M3 Sema diagnostics begin with `error:` followed by the
constraint description; parser errors begin with `error: expected`
or `error: unexpected`; preprocessor errors begin with `error:` and
mention `#`-directives or `%`-macros). For ambiguous cases (none
expected in the M1/M2/M3 corpus), the mapper falls back to
`"nsl-sema"` and logs a `DEBUG` trace identifying the orphan
diagnostic for future contract refinement.

**Forward note**: If origin disambiguation by message-prefix proves
fragile in practice, the M-track may add an explicit
`Diagnostic.origin` field in a follow-up; that's a Principle VII
coupling concern at the time it lands and is forward-compatible
with this contract.

---

## §5 `relatedInformation` field

`nsl::Diagnostic.notes` is a `std::vector<Diagnostic>` carrying
diagnostic detail (typically include-from chains per Principle IV).
Each note is mapped:

```
{
  "location": { "uri": "<note's file URI>",
                "range":  <§3 range conversion> },
  "message": "<note.message>"
}
```

Notes whose `is_include_from_note == true` carry the LSP-spec
include-from-message format: `"included from <path>:<line>:<col>"`.
Other notes carry the literal `note.message`.

`relatedInformation` is omitted (not present in the JSON) when
the diagnostic has no notes.

---

## §6 Field ordering and canonicalization

Within a single `Diagnostic` JSON object, fields MUST appear in
this order:

1. `range`
2. `severity`
3. `code`
4. `source`
5. `message`
6. `relatedInformation` (only when non-empty)

Within an array of diagnostics, ordering is by
`(range.start.line, range.start.character, severity)` ascending —
the same sort used by `DiagnosticEngine::renderAll`.

JSON serialization uses `llvm::json::OStream` with `IndentSize = 0`
(compact) for wire output; pretty-printed JSON is acceptable for
test fixtures that capture-and-diff against a golden file.

---

## §7 Edge-case behavior

### §7.1 Diagnostic with no source location

`nsl::Diagnostic.loc` is required to be valid (the `DiagnosticEngine`
contract in M1 enforces this). The mapper does NOT need to handle
an absent `loc`.

### §7.2 Diagnostic spanning multiple lines

If a diagnostic's source range spans multiple lines, the LSP
`range.end` reflects that. UTF-16 code-unit conversion is applied
per-line.

### §7.3 Diagnostic at end-of-file

If `loc` points one byte past the last byte of the file, the LSP
position is `(last-line, last-character + 1)` — LSP positions are
allowed to point one past the end of a line.

### §7.4 Diagnostic from an empty document

The mapper produces `range = {start: {0,0}, end: {0,0}}` — LSP's
canonical "no specific location in this empty document" form.

### §7.5 Multiple diagnostics at the same location

The sort order in §6 produces a stable ordering by severity for
ties on `(line, character)`: `Error(1) < Warning(2) < Information(3)`.

---

## §8 Test plan

| Test                                              | What it asserts                               |
| ------------------------------------------------- | --------------------------------------------- |
| `diagnostics_test::EmptyArrayClearsState`         | FR-012: empty `diagnostics` array clears state |
| `diagnostics_test::SingleS01`                     | One `S01` violation produces one `Diagnostic` with `code = "S01"`, `severity = 1`, `source = "nsl-sema"` |
| `diagnostics_test::CodeMapping_<Sn>` ×23          | One per `Sn` with diagnostic — SC-002 coverage |
| `diagnostics_test::SortOrder_LineThenColumn`      | Two diagnostics on same line, different columns: column-ascending |
| `diagnostics_test::SortOrder_SeverityOnTie`       | Two diagnostics at same `(line, character)`: severity-ascending |
| `diagnostics_test::IncludeFromNotes`              | Diagnostic in `#include`'d file produces `relatedInformation` with include-stack notes |
| `diagnostics_test::EditClearsResolvedDiagnostic`  | FR-012: didChange resolves error → next publish has empty array |
| `diagnostics_test::Determinism_TwoRunsByteIdentical` | SC-003: two runs over identical input produce byte-identical `publishDiagnostics` |
| `diagnostics_test::ParseError`                    | Parser-level error surfaces as `source = "nsl-parse"` |
| `diagnostics_test::PreprocessError`               | Unresolved `#include` surfaces as `source = "nsl-preprocess"` (FR-020c) |
| `diagnostics_test::UTF8Comment`                   | Diagnostic on a line containing a UTF-8 comment: UTF-16 column conversion correct |

---

## §9 Forward-compatibility commitments

- Adding a new `Sn` (`S30`, `S31`, …) MUST update §1's lookup
  table in the same PR (Principle I monotonic numbering ×
  Principle VII coupling).
- Renaming or weakening an existing `Sn` diagnostic-string MUST
  update both `specs/006-m3-sema/contracts/diagnostic-string.contract.md`
  and §1's lookup table in the same PR.
- Adding a fourth `source` value (e.g., `"nsl-lower"` once T-track
  surfaces lowering diagnostics through LSP) is a forward-compatible
  addition; existing tests are unaffected.
- Adding diagnostic `tags` (LSP 3.15+ `Unnecessary` / `Deprecated`)
  is forward-compatible; T6 lint rules will land them.
