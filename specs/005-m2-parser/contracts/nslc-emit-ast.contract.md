<!-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception -->

# Contract: `nslc -emit=ast`

**Owner**: `tools/nslc/main.cpp` (argument handling) + `lib/Driver/EmitAST.cpp` (implementation) + `include/nsl/AST/Printer.h` (format)
**Spec FRs**: FR-022, FR-023, FR-024, FR-030, FR-031, FR-032
**Spec SCs**: SC-001, SC-003, SC-007

This contract pins the user-facing behavior of `nslc -emit=ast`.
Any change to the schema, exit-code semantics, or flag interaction
is a contract amendment that requires a spec edit plus a
golden-test update under `test/Driver/emit-ast.test`. The text
format is **schema-unstable across M-track milestones** (M3, M5,
M7 may extend node lines as new node fields land — Sema-resolved
types, expansion residue, etc.); each format-bumping change
re-cuts the golden in the same patch.

## Synopsis

```
nslc [-I <dir>]... [-D <name>=<value>]... [--diagnostic-format=text|json] -emit=ast <input.nsl>
```

Environment variables consulted (inherited from M1):
- `NSL_INCLUDE` — colon-separated list of directories searched for
  `#include <…>` (angle form, per P8).

Flags inherited unchanged from M1 (do not duplicate documentation):
- `-I <dir>` — quote-form `#include` search path, repeatable.
- `-D <name>=<value>` — predefine a preprocessor macro, repeatable.
- `--diagnostic-format=text|json` — diagnostic-output format
  (text default; `json` is smoke-only at M1 per that spec's Q2).

M2 adds **only** `-emit=ast`. **No `--emit-format=*` flag is added
at M2** (per /speckit-clarify session 2026-04-27 Q2 → text-only).

## Behavior

1. Open `<input.nsl>` and load it into the `SourceManager`.
2. Run the preprocessor (per
   [M1 `preprocessor-seam.contract.md`](../../002-m1-lex-preprocess/contracts/preprocessor-seam.contract.md)).
3. Run the lexer over the post-preprocess token stream.
4. Run the parser, building a `CompilationUnit` AST. **Recovery is
   the default** (per /speckit-clarify Q1 → Option A) — the parser
   continues past syntax errors using its per-rule recovery sets,
   accumulating multiple diagnostics in the engine.
5. If the diagnostic buffer contains any `error`-severity entry,
   print all diagnostics to stderr in the format defined by
   [M1 `diagnostic-output.contract.md`](../../002-m1-lex-preprocess/contracts/diagnostic-output.contract.md)
   and exit 1. **No AST is printed on stdout in this case** —
   `EmitAST.cpp` buffers the AST text and prints only on success
   (FR-022 "no partial output").
6. Otherwise, print the AST to stdout in the canonical text format
   defined below, then exit 0. Warning-only diagnostics (e.g.,
   N10 `label`-keyword warning) are printed to stderr; the AST
   still emits.

## Stdout schema

One AST node per line. Lines are indented two spaces per parent
nesting level. Each node is wrapped in matching parentheses;
opening `(` is immediately followed by the node kind and the node
fields; the matching `)` appears on the line that closes the
node's last child (or on the same line if the node has no
children).

```
<indent>(<NodeKind>  loc=<source-range>  [<field>=<value>...]
<indent+2>...children...
<indent>)
```

Where:
- `<NodeKind>` — the `nsl::ast::NodeKind` enumerator name without
  prefix (e.g. `CompilationUnit`, `ModuleBlock`, `RegDecl`,
  `BinaryExpr`).
- `<source-range>` — `<path>:<startLine>:<startCol>-<endLine>:<endCol>`
  in **virtual** (post-`#line`) coordinates, all 1-based. The
  `<path>` is the source-manager-resolved path; for tokens after
  a `#line N "F"` directive, `<path>` is `F`.
- `<field>=<value>` — one or more node-kind-specific fields,
  whitespace-separated. Field names are documented per node kind
  in `include/nsl/AST/Printer.h`. Field-value escaping for string
  values uses C escapes (`\t`, `\n`, `\\`, `\"`).
- `<indent+2>...children...` — child nodes, recursively printed in
  the same format. Children are emitted in **declaration order**
  (the order in which the parser appended them to the parent's
  vector).

### Trailing newline

The last byte of stdout on success is `\n`.

### Example

Input `hello.nsl`:

```nsl
module hello {
  reg q[8] = 0;
}
```

Stdout (post-`#line` shown is identical to physical for this
input — no `#line` in effect):

```
(CompilationUnit  loc=hello.nsl:1:1-3:2
  (ModuleBlock  loc=hello.nsl:1:1-3:2  name=hello
    (RegDecl  loc=hello.nsl:2:3-2:16  name=q
      (LiteralExpr  loc=hello.nsl:2:9-2:10  kind=Decimal  value=8)
      (LiteralExpr  loc=hello.nsl:2:14-2:15  kind=Decimal  value=0))))
```

(Width and init are positionally distinguished by the AST: the
first child of `RegDecl` is the optional width expression, the
second is the optional init expression. If width is absent, the
first child is the init; the printer emits no placeholder. The
exact field-presence convention is golden-frozen.)

## Exit codes

| Code | Condition |
|---|---|
| 0 | Preprocess + lex + parse all succeeded with no `error`-severity diagnostics. AST printed to stdout, terminating in `\n`. Warning-severity diagnostics (if any) printed to stderr. |
| 1 | Any `error`-severity diagnostic was raised at any pipeline stage (preprocess, lex, or parse). All diagnostics printed to stderr; **no AST output on stdout**. |
| 2 | Driver argument parsing failed (e.g., `-emit=` missing operand, unknown `-emit=` value, missing input file). Usage message on stderr; no AST output. |

## Determinism guarantees (Principle V)

- Two consecutive `nslc -emit=ast` invocations on the same input
  bytes under the same flag list MUST produce **byte-identical**
  stdout (FR-030, SC-003, SC-007).
- The output MUST NOT contain raw pointer values, hash-map
  iteration order, timestamps, or any environment-derived data
  (FR-031, FR-032).
- Cross-references between AST nodes (e.g., a `goto`'s target)
  serialize as `ref=<path>:<line>:<col>` of the target's
  `SourceRange::start` — **never** as a hex pointer.
- Determinism is exercised in CI's `unit-tests` stage (golden
  byte-equality) and in the local-reproduction
  `scripts/ci.sh unit-tests`.

## Performance

Informal SLO at M2: `nslc -emit=ast` finishes in **< 1 s** on a
representative single-file input on the reference dev-container
host. No formal throughput target — measurement-driven SLOs land
when the audited corpus arrives at M7.

## Stability

The text format is **schema-unstable across M-track milestones**:

- At **M2** (this milestone): the format is frozen by
  `test/Driver/emit-ast.test`'s golden. Any unintentional change
  fails CI.
- At **M3**: Sema may add `(type=...)` annotations on `Expr`
  nodes; the golden is re-cut in the same patch.
- At **M5**: structural-expansion residue may add new node-line
  fields; same re-cut discipline.
- At **M7**: the format MAY stabilize for end-to-end CI
  consumption; that decision is made by the M7 spec.

The text format is **NOT** suitable as a stable inter-process
contract today; consumers requiring schema stability should wait
for the JSON contract that T-track will introduce (deferred per
/speckit-clarify Q2).
