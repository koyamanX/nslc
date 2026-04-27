<!-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception -->

# Contract: Parser error-recovery surface

**Owner**: `lib/Parse/Parser.cpp` (top-level driver) + `lib/Parse/Recovery.cpp` (recovery primitive + tables) + every `lib/Parse/Parse*.cpp` file (per-rule recovery sets)
**Spec FRs**: FR-019, FR-020, FR-021
**Spec SCs**: SC-002, SC-005
**Clarifying decision**: /speckit-clarify session 2026-04-27 Q1 → Option A (full multi-error recovery at every grammar level)

This contract pins the parser's error-recovery semantics. Any
change to the recovery model — disabling recovery for a rule,
changing the recovery-token set for a rule, introducing a
fail-fast mode — is a contract amendment that requires a spec
edit (FR-021) plus a fixture update under `test/parse/recovery/`.

## Recovery model

The parser uses **per-rule recovery-token sets**. At each
`parseFoo()` site that may emit a syntax-error diagnostic, the
parser knows a small `constexpr TokenSet` naming the tokens at
which to resume parsing once the error is reported. After
emitting the diagnostic, the parser invokes
`Parser::skipUntil(set)`, which advances the lexer until either:

1. `peek().kind() ∈ set`, OR
2. `peek().kind() == TokenKind::Eof`.

In case 1, the parser resumes from the current cursor position
(possibly consuming the recovery token if the rule's resume
semantics call for it). In case 2, the parser unwinds to the
nearest enclosing recovery scope; if the unwind reaches the
top-level `parseCompilationUnit()`, that function returns the
partial `CompilationUnit` (with whatever items it succeeded in
parsing) so the AST printer / future Sema can still operate on
the well-formed prefix.

## Recovery sets per grammar level

The following recovery-set table is **normative** — it pins the
default recovery tokens at each named grammar level. Per-rule
deviations are documented at the recovery site in source code
(comment block; FR-021 enforcement is "documentation lives next
to the code").

| Grammar level | Default recovery set | Resume semantics |
|---|---|---|
| `parseCompilationUnit()` (top) | `{struct, declare, module, param_int, param_str, Eof}` | Resume at the next top-level item start; do not consume the recovery token. |
| `parseDeclareItem()` (inside `declare { … }`) | `{Semi, RBrace}` | Consume the `;` if matched; if `}`, leave it for the enclosing `parseDeclareBlock()` to consume. |
| `parseModuleItem()` (inside `module { … }`) | `{Semi, RBrace, func, function, proc, state, wire, reg, mem, integer, variable, proc_name, state_name, first_state, func_self}` | Consume the `;` if matched; otherwise leave the recovery token in place for the next iteration. |
| `parseSeqItem()` (inside `seq { … }`) | `{Semi, RBrace, if, for, while, goto, return}` | Consume the `;` if matched. |
| `parseStmt()` (statement-position dispatch) | inherited from enclosing item-list | Falls back to enclosing rule's recovery set. |
| `parseExpr()` (expression-position dispatch) | inherited from enclosing statement / decl | Falls back to enclosing rule's recovery set. |

The actual `TokenKind` enumerator names (e.g., `TokenKind::Module`,
`TokenKind::Semi`) are sourced from `nsl-lex`'s public
`TokenKind.h`; the tables in `lib/Parse/Recovery.cpp` use those
names directly.

## Multi-error fixture corpus minimum

Per FR-021, the repository MUST carry at least these three
multi-error fixtures under `test/parse/recovery/`:

1. **`two-top-level-errors.nsl`** — two independent syntax errors
   in separate `top_level_item`s (e.g., a malformed `struct` and a
   malformed `module`). Asserted: both diagnostics emitted in
   source order; well-formed `top_level_item`s between/after the
   errors still appear in the AST.
2. **`two-module-item-errors.nsl`** — one `module { … }` whose
   body contains two independent syntax errors in separate
   `module_item`s (e.g., a malformed `reg` declaration and a
   malformed `func` definition). Asserted: both diagnostics
   emitted in source order; `module_item`s between/after the
   errors still appear in the `ModuleBlock` AST node.
3. **`error-in-seq-followed-by-module-item.nsl`** — a `module`
   body containing a malformed expression inside a `seq` block,
   followed by a well-formed `module_item` after the `seq` block.
   Asserted: the `seq`-block diagnostic emitted; the well-formed
   subsequent `module_item` still appears in the AST.

Additional fixtures are encouraged where they exercise specific
recovery paths (e.g., recovery inside a Pratt expression parse,
recovery across nested `alt`/`any` blocks).

## Diagnostic output during recovery

Every recovery-emitted diagnostic uses the canonical M1 format
(per
[M1 `diagnostic-output.contract.md`](../../002-m1-lex-preprocess/contracts/diagnostic-output.contract.md)):

```
<path>:<line>:<col>: error: <message>
```

The `<message>` is parser-specific — examples:

- `expected ';' after register declaration`
- `expected '}' to close 'module' body`
- `expected expression after binary operator`
- `'label' is reserved; using as identifier (parser-note N10)` (warning, not error)
- `'#line' directive must be followed by a positive integer (parser-note N14)`

The `<message>` text is golden-frozen per fail-case fixture (per
research §10).

## What recovery does NOT do

- **Recovery does NOT silence subsequent errors.** Multi-error
  reporting is the explicit goal.
- **Recovery does NOT produce synthetic AST nodes** for the
  malformed construct. The malformed construct is simply absent
  from the parent's vector; well-formed siblings remain.
- **Recovery does NOT propagate downstream.** M3 sema, M4
  dialect, etc. all see the partial AST and either accept it
  (if Sema can analyze the well-formed prefix) or refuse to run
  (if the diagnostic count is non-zero — the standard
  early-exit pattern).

## Forbidden behaviors (Constitution alignment)

- **Recovery MUST NOT swallow** an error — every error site MUST
  emit a diagnostic via the `DiagnosticEngine` (Principle IV).
  Silent recovery is a bug.
- **Recovery MUST NOT introduce non-determinism** — the recovery
  set is a `constexpr` bitset; the `skipUntil()` walk is a
  deterministic forward scan (Principle V).
- **Recovery MUST NOT bypass the diagnostic engine.** Direct
  `stderr` writes from `lib/Parse/` are forbidden (FR-019,
  re-statement of M1 FR-024).

## Future evolution (not in M2 scope)

- **Synthetic placeholder nodes.** A future LSP-driven UX
  improvement might inject a `RecoveryStmt` placeholder where a
  malformed statement was, so hover/completion features still
  have *something* to traverse. This requires a new node kind
  and a contract amendment; explicitly not at M2.
- **Tunable recovery aggressiveness.** A future flag (e.g.,
  `--max-errors=N`) might cap recovery to N errors before
  bailing. Not at M2; Q1's decision was full recovery, not
  tunable recovery.
- **Recovery telemetry.** A future debug flag might dump the
  recovery-set push/pop trace to stderr for parser-development
  diagnostics. Not at M2.
