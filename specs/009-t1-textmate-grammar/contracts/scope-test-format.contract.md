<!-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception -->

# Contract — Scope-Test Format

**Feature**: `009-t1-textmate-grammar`
**Phase**: 1 (design / contracts)
**Date**: 2026-05-04

This contract freezes the **fixture-and-assertion file format** the
T1 scope-test runner accepts and how `./scripts/ci.sh` invokes it.
Pinning the format lets future T-track milestones (T8 tree-sitter
in particular) follow the same mechanical regression pattern.

Runner: `vscode-tmgrammar-test` per research.md §1.

---

## §1 Fixture file shape

Each fixture is an `.nsl` file under
`test/tooling/textmate/fixtures/`. It contains:

1. **Real NSL surface syntax** — at least one occurrence of every
   token under test in that file's category.
2. **Inline assertion comments** prefixed with `// >` (and
   variant `// <-`) per the `vscode-tmgrammar-test` DSL.
3. **No** runtime / executable behaviour — these files are *not*
   compiled by `nslc`. The grammar runs over the raw bytes.

### 1.1 Assertion DSL

The runner accepts two assertion forms inside `//`-style comments:

```nsl
declare hello {}
// <- keyword.declaration.nsl
//      ^^^^^ entity.name.declaration.nsl   <-- (out of T1 scope; commented out)
```

| Form | Effect |
|---|---|
| `// <- <scope>` | Asserts that the scope at column 1 of the **previous** line equals `<scope>`. |
| `//      ^^^^^ <scope>` | Asserts that the scope across the marked range (`^` characters) of the previous line equals `<scope>`. The leading whitespace + `^^^^^` together specify column-start and column-end. |

A single fixture line may have **multiple** following assertion
lines targeting different ranges:

```nsl
reg q[8] = 8'hFF;
// <- storage.type.register.nsl
//        ^ constant.numeric.decimal.nsl
//             ^^^^^ constant.numeric.verilog.nsl
```

Comments without the `<-` / `^^^` markers are ordinary NSL line
comments and **not** assertions; they receive
`comment.line.double-slash.nsl` like any NSL comment.

### 1.2 What goes in each fixture file

| File | Coverage requirement |
|---|---|
| `all-keywords.nsl` | ≥ 1 occurrence per `KeywordSet.def` row, plus an assertion per row. **Generated** by `scripts/gen_textmate_fixtures.py`. |
| `all-numbers.nsl` | One example per numeric form in grammar-coverage.contract.md §3 (5 forms × Verilog-sized × `b`/`o`/`d`/`h` permutations); one with `Z`/`X`/`U` markers; one with underscore separators. Hand-authored. |
| `all-operators.nsl` | One example per operator category in grammar-coverage.contract.md §5 (8 categories) with the multi-character variants (`==`, `<=`, `&&`, etc.). Hand-authored. |
| `all-directives.nsl` | One line per directive in grammar-coverage.contract.md §6 (9 directives). Hand-authored. |
| `comments-and-strings.nsl` | One line comment, one block comment, one string with backslash escape, one block comment containing a keyword spelling, one string containing a keyword spelling. Hand-authored. |
| `macro-references.nsl` | At least one `%IDENT%` reference inside a `reg` declaration's width specification (matching the canonical use in `pp.ebnf §4` examples). Hand-authored. |

### 1.3 Generated-vs-hand-authored separation

`all-keywords.nsl` is **generated** because `KeywordSet.def`
already drives multiple consumers (research.md §6) and
mechanically tracking that file's set into a fixture is
constitutionally required (Principle VII coupling).

The other five fixtures are **hand-authored** because their
coverage axes (numeric forms / operator categories / directives
/ comment-string-edge-cases / macro-reference) are not enumerated
in any single `.def` file — they are spec-grammar cross-cuts that
do not benefit from generation.

---

## §2 Assertion file shape (`vscode-tmgrammar-test` test
specification)

Each fixture has a sibling `.spec` file under
`test/tooling/textmate/scope-tests/` carrying the runner-level
configuration. Format per the `vscode-tmgrammar-test` README:

```yaml
# all-keywords.spec
fixture: ../fixtures/all-keywords.nsl
grammar: ../../../grammars/textmate/nsl.tmLanguage.json
language-id: nsl
scope-name: source.nsl
```

The `.spec` file is small (4 lines per fixture); the assertions
themselves live inside the fixture's `// <-` and `// ^^^` lines
(per §1.1). Splitting the runner config into `.spec` files lets
each fixture be re-used by future tools that would consume it
without the runner (e.g. a grammar inspector script).

---

## §3 Runner invocation contract

The runner is invoked from `./scripts/ci.sh` stage 3:

```bash
./scripts/ci.sh tooling-textmate
```

Internally:

```bash
npx vscode-tmgrammar-test \
  --grammar grammars/textmate/nsl.tmLanguage.json \
  --tests   "test/tooling/textmate/scope-tests/*.spec"
```

| Exit code | Meaning |
|---|---|
| `0` | All assertions in all `.spec` files passed. |
| non-zero | At least one assertion failed; the runner prints the failing assertion's file/line/column and the observed-vs-expected scope. |

Stage 3 in `./scripts/ci.sh` MUST treat any non-zero exit code
from this sub-step as a stage-3 failure (which fails the entire
CI run per Principle IX no-bypass).

---

## §4 Independence from compiler

Per FR-019: the runner MUST execute correctly on a clean checkout
without a built `nslc`. Concretely:

- The `vscode-tmgrammar-test` package is the only runtime
  dependency for this test layer; it ships pure JavaScript
  + Oniguruma WASM bindings inside the npm package. No
  C++ link step, no `libNSLFrontend.a` consumption.
- The `.spec` files reference `grammars/textmate/nsl.tmLanguage.json`
  by path; the path resolves regardless of build state.
- The fixture files are `.nsl`-extension plain text; the file
  extension does NOT cause the runner to invoke the compiler — the
  runner only feeds bytes to the grammar.

CI verification: a dedicated stage-3 sub-step runs the tooling
tests **before** the compiler test step, on a `make clean`
artefact tree. If the order accidentally reverses (compiler tests
first), a developer who breaks the tooling tests but not the
compiler still sees a stage-3 failure.

---

## §5 Failure-mode rendering

The runner's failure output looks like:

```
✗ test/tooling/textmate/fixtures/all-keywords.nsl:7
  Expected:  storage.type.register.nsl
  Got:        (no scope)
  Source:    `reg q;`
                ^^^
```

CI captures this verbatim into the failed-stage log. The
contributor running `./scripts/ci.sh tooling-textmate` locally
sees the same output — Principle IX reproducibility holds.

---

## §6 Adding a new assertion

When a `KeywordSet.def` edit lands:

1. Run `python3 scripts/gen_textmate_fixtures.py` —
   regenerates `all-keywords.nsl` (and its embedded
   assertions) to include the new keyword.
2. Run `python3 scripts/gen_textmate_grammar.py` —
   regenerates `nsl.tmLanguage.json` to include the new
   keyword pattern.
3. Run `./scripts/ci.sh tooling-textmate` locally — should
   pass on green.
4. Commit all three regenerated artefacts (fixture, grammar,
   `KeywordSet.def`) in the same PR per Principle VII coupling.

When a new operator / directive / numeric form is added by an
NSL spec amendment:

1. Hand-edit the relevant fixture
   (`all-operators.nsl` / `all-directives.nsl` / `all-numbers.nsl`)
   to add the new example + assertion(s).
2. Hand-edit `scripts/gen_textmate_grammar.py`'s inline JSON
   template to add the new pattern.
3. Update `contracts/grammar-coverage.contract.md` (this contract
   plus the coverage contract) to record the new binding.
4. Run `./scripts/ci.sh tooling-textmate` locally — should pass.
5. Commit all artefacts in the same PR.

---

## §7 What this contract intentionally does NOT cover

- **Tree-sitter parse-tree assertions** — T8's analogous file
  format is the `tree-sitter` corpus format (`test/highlight/`),
  not the `vscode-tmgrammar-test` `// >` form. T8 may borrow
  this contract's structure but will introduce its own
  `*-tree-sitter.contract.md`.
- **LSP semantic-token assertions** — T4's analogous test layer
  asserts on `textDocument/semanticTokens` responses; uses LSP
  protocol tests, not scope-test assertions.
- **Performance benchmarks** — SC-006 ("≤ 10 s on a standard CI
  runner") is a wall-clock assertion enforced by the CI step's
  timeout, not by an in-runner benchmark. The runner does not
  emit perf measurements.
