<!-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception -->

# Contract: `DiagnosticEngine` output formats

**Owner**: `lib/Basic/Diagnostic.cpp` (renderer) + `include/nsl/Basic/Diagnostic.h` (API)
**Spec FRs**: FR-024, FR-025, FR-026, FR-027, FR-028
**Spec SCs**: SC-004, SC-006

This contract pins the **canonical text format** and the
**smoke-only NDJSON format** of diagnostics emitted by any layer in
`libNSLFrontend.a` at M1. The lexer, preprocessor, and (later) parser
/ sema / MLIR pass / CIRCT pass / ExportVerilog all route through
this engine — direct writes to `stderr` are forbidden (FR-024).

The JSON schema is **smoke-only at M1** per /speckit-clarify session
2026-04-27 Q2; full schema lock + per-kind round-trip goldens defer
to T3.

## Text format (canonical)

Each diagnostic renders as one or more lines:

```
<path>:<line>:<col>: <severity>: <message>
[<source line text>]
[<caret pointer>]
[note: included from <ancestor-path>:<ancestor-line>]...
[note: included from <next-ancestor-path>:<next-ancestor-line>]...
```

Where:
- `<path>` — the file as understood by the `SourceManager` AT
  EMISSION (i.e., post-`#line` adjustment if applicable). 100% of
  diagnostics MUST match the regex `^[^:]+:\d+:\d+: (error|warning|note): .+$`
  (SC-004).
- `<line>`, `<col>` — 1-based.
- `<severity>` — exactly one of `error`, `warning`, `note`. Lowercase.
- `<message>` — the diagnostic text. Non-empty. Names the offending
  construct (e.g., `unterminated string literal`, not `lex error`)
  per FR-037.
- `[<source line text>]` — the literal source line containing the
  offending location. Optional at M1 (renderer SHOULD include it
  when terminal width permits; tasks.md may scope a flag to suppress).
- `[<caret pointer>]` — a `^` (or `^~~~~` for ranges) pointing at
  the offending column. Always paired with the source line.
- `[note: included from ...]` — one trailing line per ancestor
  file in the include stack (FR-026). The order is innermost-first:
  the file containing the directly-`#include`'d offender is on the
  first `note:` line, then its parent, etc.

### Example: lex error in nested include

Diagnostic emitted in `inner.nsl:5:10:` with include chain
`a.nsl` → `b.nsl` → `inner.nsl`:

```
inner.nsl:5:10: error: unterminated string literal
    reg x = "hello
             ^
note: included from b.nsl:3:1
note: included from a.nsl:7:1
```

### Example: preprocessor error with `#line` adjustment

If `#line 1000 "synth.nsl"` was active, an error two lines later:

```
synth.nsl:1002:5: error: '#endif' without matching '#if'
  #endif
  ^~~~~~
```

## NDJSON format (smoke-only at M1)

One JSON object per line, newline-delimited (RFC 7464). Each object
has at minimum these five fields:

```json
{"path":"<string>","line":<int>,"col":<int>,"severity":"<error|warning|note>","message":"<string>"}
```

Include-stack notes appear as separate NDJSON objects with
`"severity":"note"` and an additional `"included_from"` field:

```json
{"path":"b.nsl","line":3,"col":1,"severity":"note","message":"included from","included_from":{"path":"b.nsl","line":3,"col":1}}
```

### M1 smoke-test invariants only

The `test/preprocess/include-stack/` and analogous fixtures verify:

- Each emitted line is valid JSON (parses with any compliant parser).
- Each object has the five mandatory fields, with the right types
  (`path`: string non-empty; `line`/`col`: positive integer;
  `severity`: one of three strings; `message`: string non-empty).
- For include-stack notes, the `included_from` field exists and is
  a `{path, line, col}` object.

**No** content-equality assertions. **No** schema-versioning. **No**
per-diagnostic-kind discriminator. All deferred to T3 against a real
LSP consumer.

### Example NDJSON for the same nested-include error

```
{"path":"inner.nsl","line":5,"col":10,"severity":"error","message":"unterminated string literal"}
{"path":"b.nsl","line":3,"col":1,"severity":"note","message":"included from","included_from":{"path":"b.nsl","line":3,"col":1}}
{"path":"a.nsl","line":7,"col":1,"severity":"note","message":"included from","included_from":{"path":"a.nsl","line":7,"col":1}}
```

## Routing

- **`stderr`** is the destination for all diagnostic output, both
  text and JSON, in `nslc -emit=tokens`. Stdout is reserved for the
  successful token stream.
- The format is selected by the CLI flag
  `nslc --diagnostic-format={text|json}`; the default is `text`.
  The flag may appear before or after `-emit=tokens` (order-
  insensitive).

## Determinism (FR-038, Principle V)

- Diagnostics MUST render in **`(SourceLocation, Severity)` order**
  regardless of the order they were emitted by the producing layer
  (research §4 — sort-on-render in `DiagnosticEngine::renderAll`).
  This catches the "two passes raised in different orders due to
  early-out" failure mode.
- The engine internally stores an unsorted `std::vector<Diagnostic>`
  for emit-time efficiency; sorting happens in `renderAll`. Two
  consecutive `renderAll` calls on the same buffer MUST produce
  byte-identical output.

## Mandatory diagnostic-string fixtures (FR-037)

Per FR-037, fail-fixtures for the following rules MUST cite the
exact diagnostic message string. The message strings are:

| Rule | Message (M1 lock) |
|------|-------------------|
| P3   | `undefined macro reference: '%<NAME>%'` |
| P6   | `compile-time helper '_<NAME>' used outside #define / #if condition` |
| P7   | `float literal cannot cross the preprocessor seam` |
| P9   | `'#endif' without matching '#if' / '#ifdef' / '#ifndef'` (and the symmetric `unterminated #if at end of file`) |
| Lex unterminated string | `unterminated string literal` |

Renaming or weakening any of these strings later requires a
contract amendment in this file PLUS updating the corresponding
fail-fixture.
