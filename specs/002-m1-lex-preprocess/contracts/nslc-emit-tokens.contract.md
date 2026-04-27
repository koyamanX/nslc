<!-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception -->

# Contract: `nslc -emit=tokens`

**Owner**: `tools/nslc/main.cpp` (argument handling) + `lib/Driver/EmitTokens.cpp` (implementation)
**Spec FRs**: FR-029, FR-030, FR-038
**Spec SCs**: SC-001, SC-005

This contract pins the user-facing behavior of `nslc -emit=tokens`.
Any change to the schema, exit-code semantics, or flag interaction
is a contract amendment that requires a spec edit (FR-029) plus a
golden-test update under `test/Driver/emit-tokens.test`.

## Synopsis

```
nslc [-I <dir>]... [-D <name>=<value>]... -emit=tokens <input.nsl>
```

Environment variables consulted:
- `NSL_INCLUDE` — colon-separated list of directories searched for
  `#include <…>` (angle form, per P8).

## Behavior

1. Open `<input.nsl>` and load it into the `SourceManager`.
2. Run the preprocessor over the loaded buffer (full directive set;
   see [`preprocessor-seam.contract.md`](./preprocessor-seam.contract.md)).
3. Run the lexer over the post-preprocess token stream.
4. Print each emitted token to stdout in the canonical format
   defined below.
5. Print the diagnostic buffer (if non-empty) to stderr in the
   format defined by [`diagnostic-output.contract.md`](./diagnostic-output.contract.md).
6. Exit 0 if the diagnostic buffer contains no `error`-severity
   entries; exit 1 otherwise. **No partial token output is printed
   on error** — `EmitTokens.cpp` buffers the token stream and prints
   only on success.

## Stdout schema

One token per line. Fields are tab-separated. Trailing newline after
each line.

```
<kind>\t<spelling>\t<phys-loc>\t<virt-loc>\t<flags>
```

Where:
- `<kind>` — the `TokenKind` enumerator name (e.g. `tk_module`,
  `tk_identifier`, `tk_decimal_lit`, `tk_line_directive`). The
  `tk_eof` token is printed as the final line.
- `<spelling>` — the literal source text the token spans, with
  embedded tabs / newlines / backslashes / double-quotes
  C-escaped (`\t`, `\n`, `\\`, `\"`). Always non-empty except for
  `tk_eof`.
- `<phys-loc>` — `<path>:<line>:<col>:<offset>` in the **physical**
  source file (resolved through the `SourceManager`'s buffer table,
  ignoring `#line` overrides). All four fields are 1-based except
  offset, which is 0-based.
- `<virt-loc>` — `<path>:<line>:<col>` in the **virtual** source
  position (post-`#line` adjustment). Identical to `<phys-loc>`'s
  `<path>:<line>:<col>` if no `#line` directive is in effect at
  this token's position.
- `<flags>` — a bracketed comma-separated list of flag names. Empty
  brackets `[]` if none. Numeric tokens may carry `Z`, `X`, `U`
  flags reflecting the digits actually present in the literal.
  Identifiers and keywords always print `[]`.

### Example

Input `t.nsl`:

```nsl
module foo {
  reg w[8] = 0xZ_F;
}
```

Stdout:

```
tk_module	module	t.nsl:1:1:0	t.nsl:1:1	[]
tk_identifier	foo	t.nsl:1:8:7	t.nsl:1:8	[]
tk_lbrace	{	t.nsl:1:12:11	t.nsl:1:12	[]
tk_reg	reg	t.nsl:2:3:15	t.nsl:2:3	[]
tk_identifier	w	t.nsl:2:7:19	t.nsl:2:7	[]
tk_lbracket	[	t.nsl:2:8:20	t.nsl:2:8	[]
tk_decimal_lit	8	t.nsl:2:9:21	t.nsl:2:9	[]
tk_rbracket	]	t.nsl:2:10:22	t.nsl:2:10	[]
tk_assign	=	t.nsl:2:12:24	t.nsl:2:12	[]
tk_hex_lit	0xZ_F	t.nsl:2:14:26	t.nsl:2:14	[Z]
tk_semicolon	;	t.nsl:2:19:31	t.nsl:2:19	[]
tk_rbrace	}	t.nsl:3:1:33	t.nsl:3:1	[]
tk_eof			t.nsl:3:2:34	t.nsl:3:2	[]
```

## Exit codes

| Code | Condition |
|------|-----------|
| 0    | Preprocess + lex completed; diagnostic buffer has no `error` entries. |
| 1    | Preprocess or lex raised at least one `error`. **No tokens are printed on stdout.** Diagnostics on stderr. |
| 2    | Argument parse failure (e.g., `-emit=` value not recognized, `-I` missing argument). Usage on stderr. |
| 3    | Input file could not be opened (does not exist, permission denied, …). Diagnostic on stderr. |

## Determinism guarantees (Principle V, FR-038)

- Two invocations with the same `<input.nsl>` content + same
  `-I` / `-D` flag list + same `NSL_INCLUDE` value MUST produce
  byte-identical stdout.
- The build environment (CWD, hostname, mtime, locale, env vars
  other than `NSL_INCLUDE`) MUST NOT influence stdout.
- Token order MUST match the source order; the macro-table
  iteration order MUST be insertion-order (research §4).

## Performance budget (informal at M1)

- A single-file fixture under 1 KiB SHOULD complete in under
  100 ms on the reference Linux x86_64 host (no SLO; informal —
  budget revisited at M7 with audited-corpus measurement basis).

## Negative test list (golden under `test/Driver/`)

- `nslc -emit=tokens` with no input → exit 2, "input file required"
  on stderr.
- `nslc -emit=tokens nonexistent.nsl` → exit 3, "could not open"
  on stderr.
- `nslc -emit=blarg input.nsl` → exit 2, "unknown emit stage" on
  stderr.
- `nslc -emit=tokens input.nsl` where `input.nsl` has a lex error →
  exit 1, no tokens on stdout, diagnostic on stderr citing the
  error.
- `nslc -emit=tokens input.nsl` where preprocess raises an error
  inside an `#include`'d file → exit 1, diagnostic cites the
  inner file's `path:line:col`, include-stack notes follow.

## Compatibility note

`-emit=tokens` is the M1 increment to a forward-extensible
`-emit=<stage>` flag (Principle V). Future M2 `-emit=ast`, M5
`-emit=mlir`, M6 `-emit=hw`, and M7 `-emit=verilog` reuse the same
argument-handling shape. Adding a new `-emit=` value is a single
case-arm addition in `tools/nslc/main.cpp` plus a new `lib/Driver/Emit*.cpp`.
