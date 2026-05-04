<!-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception -->

# Contract: Formatting rules (T2 freeze)

**Branch**: `010-t2-formatter-v0` | **Date**: 2026-05-04
**Plan**: [../plan.md](../plan.md) | **Spec**: [../spec.md](../spec.md)

This contract freezes the six NSL-specific formatting rules
listed in
[`docs/design/nsl_tooling_design.md`](../../../docs/design/nsl_tooling_design.md)
§5.3 as machine-checkable input/output assertions. Each rule
gets a paired pre-format / post-format golden file under
`test/Fmt/rules/<rule>/`; the rule contract here freezes the
*post-format* shape. Reformatting any of the post-format
goldens MUST produce byte-identical output (idempotence,
FR-008).

---

## §1. Rule R1: `alt`/`any` case-arrow alignment

**Source (canonical post-format)**:

```nsl
alt {
    state == IDLE    : reg := 0;
    state == RUNNING : reg := 1;
    state == DONE    : reg := 2;
}
```

**Frozen invariants**:
- The `:` separators in all cases of an `alt` or `any` block
  align in the same source column.
- The column is determined by the LONGEST condition expression
  in the block (one space follows the longest condition; shorter
  conditions are padded with spaces on the right).
- If `align_case_arrows = false`, alignment is suppressed: each
  case gets exactly one space between its condition and the
  `:`.
- If `preserve_comments = all`, a trailing `// comment` after a
  case body MUST stay on the same line as that case (per R6).

**Test fixture**: `test/Fmt/rules/alt-case-alignment/`
- `pre.nsl` — non-aligned input
- `post.nsl` — frozen post-format output (the FileCheck pattern)
- `idempotence.nsl` — re-running on `post.nsl` MUST produce
  `post.nsl`

---

## §2. Rule R2: struct member-bracket alignment

**Source (canonical post-format)**:

```nsl
struct csr_t {
    mstatus  [32];
    mcause   [32];
    mtvec    [30];
    mepc     [32];
};
```

**Frozen invariants**:
- The `[N]` bit-width brackets in all members of a struct
  align in the same source column.
- The column is determined by the LONGEST member name in the
  struct (one space follows the longest name; shorter names
  are padded).
- If `align_struct_members = false`, alignment is suppressed:
  exactly one space between member name and `[`.
- Struct members WITHOUT a bit width (e.g. `member_name;`) do
  NOT participate in the alignment calculation; they are
  emitted as-is.

**Test fixture**: `test/Fmt/rules/struct-member-alignment/`

---

## §3. Rule R3: `proc_name` argument-list wrapping

**Source (canonical post-format)** — multi-line form when at
least one arg has a width:

```nsl
proc_name exec(
    pc   [32],
    inst [32],
    src1 [32],
    src2 [32]
);
```

**Source (canonical post-format)** — single-line form when no
arg has a width AND total fits within `max_line_length`:

```nsl
proc_name simple(a, b, c);
```

**Frozen invariants**:
- If ANY argument has a `[N]` width, every argument goes on its
  own line, indented one `indent` level past the opening `(`.
- Within the multi-line form, the `[N]` widths align in the
  same column (rule R2 applied to argument names).
- The closing `)` and `;` go on a new line at the original
  indent level (matches the `(` column).
- The single-line form is used iff all args lack widths AND
  the total reformatted line fits within `max_line_length`.
- Trailing-comma policy follows `trailing_commas`: with
  `Add`, the last arg gets a trailing comma in multi-line form;
  with `Remove`, no trailing comma; with `Preserve`, the
  pre-format trailing-comma state is kept.

**Test fixture**: `test/Fmt/rules/proc-name-arg-wrap/`

---

## §4. Rule R4: bit-slice and concat spacing

**Source (canonical post-format)**:

```nsl
wire a [8];
wire b [8];
wire c [16];

c = {a, b};
wire d = a[7:0];
wire e = b[7:4];
```

**Frozen invariants**:
- Bit slice: `a[7:0]` — NEVER `a[ 7:0 ]` or `a[7 : 0]`. No
  spaces inside `[`, `]`, around `:`.
- Concatenation: `{a, b, c}` — one space after each `,`, NO
  space inside `{`, `}` (UNLESS `spaces_inside_braces = true`,
  in which case one space inside both).
- Empty bit-slice (`a[7:0]`) and one-element concat (`{a}`)
  follow the same spacing.
- Nested forms: `{a, b[7:0], c[3:1]}` — spacing applies
  recursively.

**Test fixture**: `test/Fmt/rules/bit-slice-spacing/`

---

## §5. Rule R5: operator spacing

**Source (canonical post-format)**:

```nsl
wire x = a + b;
wire y = ~c;
wire z = !cond ? d : e;
wire w = (a + b) * (c - d);
```

**Frozen invariants**:
- Binary operators (`+`, `-`, `*`, `/`, `%`, `==`, `!=`,
  `<`, `<=`, `>`, `>=`, `&&`, `||`, `&`, `|`, `^`, `<<`,
  `>>`): one space on each side.
- Unary operators (`~`, `!`, `-` as negation, `+` as
  identity): NO space between operator and operand.
- Conditional operator (`?` `:`): one space around `?`, one
  space around `:`.
- Sign-extend `#` and zero-extend `'` operators: NO space
  between operator and following identifier (e.g. `#a`,
  `'b`). Reason: per `nsl_lang.ebnf` §11 these are unary-like
  prefix operators; spacing matches `~`/`!`.
- If `spaces_around_binary_ops = false`, binary operators get
  zero spaces (used by some legacy projects).

**Test fixture**: `test/Fmt/rules/operator-spacing/`

---

## §6. Rule R6: attached-comment preservation

**Source (canonical post-format)**:

```nsl
// Block comment above the declaration stays above.
reg foo[8];

reg bar[8];   // Trailing line comment stays on the same line.

/* Block comment above
   spanning multiple lines
   stays above. */
reg baz[8];
```

**Frozen invariants**:
- A `LineComment` on the same source line as a declaration
  (separated only by whitespace) MUST be emitted on the same
  line as that declaration in the formatted output.
- A `LineComment` on its own line above a declaration MUST be
  emitted on its own line above the declaration in the
  formatted output, with at most one blank line between them
  (preserving paragraph breaks).
- A `BlockComment` immediately above a declaration MUST stay
  immediately above the declaration.
- A `BlockComment` between two same-line tokens MUST be
  emitted on the same line in the same position.
- If `preserve_comments = leading_only`, trailing
  `LineComment`s are dropped (rare; some projects); if
  `preserve_comments = none`, all comments are dropped (very
  rare).
- Blank lines (sequences of newline trivia) between
  declarations are preserved up to a maximum of
  `blank_lines_between_modules` between top-level constructs;
  internal blank lines are clamped to one.

**Test fixture**: `test/Fmt/rules/attached-comments/`

---

## §7. Refusal-mode frozen diagnostic strings

The following diagnostic strings are frozen for the Principle
VIII string-stability rule. Renaming any of them later requires
updating the matching lit fixture.

| Trigger | Diagnostic string (frozen) |
|---|---|
| Parse error in NSL fragment | `error: nsl-fmt: parse error in <file>:<line>:<col>; refusing to format` |
| `--range LINE:LINE` out of bounds | `error: --range <a>:<b> falls outside file (file has <N> lines)` |
| `--range LINE:LINE` invalid syntax | `error: --range expects LINE:LINE (1-indexed, inclusive)` |
| Mutually-exclusive flags | (one of the five frozen strings in [`cli-surface.contract.md`](./cli-surface.contract.md) §2) |
| Unknown TOML key | `warning: unknown configuration key '<key>' at <file>:<line>; ignoring` |
| Out-of-range TOML value | `error: configuration value for '<key>' must be <expected>; got <actual> at <file>:<line>` |
| Configuration file not found (with `--config <path>`) | `error: configuration file not found: <path>` |

Each string is keyed to a specific lit fixture under
`test/Fmt/edge/`, `test/Fmt/cli/`, or `test/Fmt/config/`.
Renaming a string later: amend the fixture in the same change.

---

## §8. Rule-vs-config interaction matrix

| Rule | Affected by config keys |
|---|---|
| R1 (alt-arrow alignment) | `align_case_arrows` |
| R2 (struct member alignment) | `align_struct_members`, `indent`, `max_line_length` |
| R3 (proc_name wrapping) | `align_struct_members`, `indent`, `max_line_length`, `trailing_commas` |
| R4 (bit-slice / concat spacing) | `spaces_inside_braces` |
| R5 (operator spacing) | `spaces_around_binary_ops` |
| R6 (attached-comment preservation) | `preserve_comments`, `blank_lines_between_modules` |

`brace_style` (`KAndR` vs `Allman`) affects every rule that
emits a `{` (R1, R2, R3, control-flow blocks, module bodies);
its effect is global to the renderer rather than per-rule.

---

## Spec cross-reference

| Spec FR / SC | This contract section |
|---|---|
| FR-008 (idempotence) | All rules have an `idempotence.nsl` fixture |
| FR-009 (apply six §5.3 rules) | §1–§6 (one section per rule) |
| FR-010 (preserve comments) | §6 |
| FR-011 (preserve numeric literals) | §4 (literal preservation in slices) + R5 |
| FR-012 (refuse on parse error) | §7 (frozen diagnostic string) |
| FR-014 (10 config keys) | §8 (rule ↔ key interaction matrix) |
| Principle VIII (string stability) | §7 |
