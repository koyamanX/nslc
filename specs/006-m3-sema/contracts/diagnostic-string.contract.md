<!-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception -->

# Contract: Frozen diagnostic-message strings (S1–S29)

**Owner**: `lib/Sema/Constraints/S<NN>_*.cpp` (one source per
constraint family)
**Spec FRs**: FR-011, FR-012, FR-015
**Spec SCs**: SC-001, SC-002, SC-006
**Constitutional anchors**: Principle VIII (TDD `Sn`/`Nn`/`Pn` clause — diagnostic-string MUST be cited in fail-case)

This contract is the authoritative freeze surface for every
diagnostic message text emitted by Sema. Per Constitution Principle
VIII, the `s<NN>/fail.nsl` fixture's literal-string assertion is
what locks the message; this contract is the human-readable index of
those frozen strings, used by code review and by the spec-coupling
audit (Principle VII).

The 23 *error/warning* `Sn` (FR-010 rows whose Severity column is
"Error" or "Warning") have a frozen string each. The 6 *constructive*
`Sn` (`S13`, `S18`, `S19`, `S23`, `S24`, `S27` — Severity column
"Layout" / "Classification" / "Lowering") emit NO diagnostic and
therefore have NO row in this table; they are tested via paired
introspection per Clarifications session 2026-04-28 Q1 → Option B
(see `sema-api.contract.md` Invariant 4 and
`test_unit/constructive_sn_test/`).

## Diagnostic format

Every Sema diagnostic renders as:

```
<path>:<line>:<col>: <severity>: <message> (S<NN>)
```

The `(S<NN>)` suffix is the spec marker per FR-011, frozen across
all 23 messages.

## Frozen messages (23)

| `Sn` | Severity | `<message>` (verbatim text) | Fix-it? |
|---|---|---|---|
| `S1`  | error   | `identifier may not contain '__'`                                                                                  | no |
| `S2`  | error   | `'wire' may not have an initializer; use 'reg' instead`                                                            | no |
| `S3`  | error   | `'=' targets a wire, output, inout, variable, or integer; use ':=' for reg`                                        | yes |
| `S3`  | error   | `':=' targets a reg or struct-instance-reg; use '=' for wire/output/inout/variable/integer`                        | yes (mirrored) |
| `S4`  | error   | `dummy argument of 'func_in' must be declared 'input'`                                                              | no |
| `S4`  | error   | `dummy argument of 'func_out' must be declared 'output'`                                                            | no (mirrored) |
| `S4`  | error   | `dummy argument of 'func_self' must be declared 'wire'`                                                             | no (mirrored) |
| `S5`  | error   | `return-value terminal of 'func_in' must be declared 'output' or 'inout'`                                           | no |
| `S5`  | error   | `return-value terminal of 'func_out' must be declared 'input' or 'inout'`                                           | no (mirrored) |
| `S6`  | error   | `'proc_name' arguments must be 'reg' identifiers`                                                                   | no |
| `S7`  | error   | `'seq' block may appear only inside a function or procedure body`                                                   | yes (omit-when-non-removable) |
| `S7`  | error   | `'while' block may appear only inside a function or procedure body`                                                 | yes (mirrored) |
| `S7`  | error   | `'for' block may appear only inside a function or procedure body`                                                   | yes (mirrored) |
| `S8`  | error   | `'while' block may appear only inside a 'seq' block`                                                                | no |
| `S8`  | error   | `'for' block may appear only inside a 'seq' block`                                                                  | no (mirrored) |
| `S9`  | error   | `for-loop variable must be a 'reg' identifier`                                                                      | no |
| `S10` | error   | `'generate' loop variable must be an 'integer' identifier`                                                          | no |
| `S11` | error   | `'state_name' is scoped to its enclosing 'proc' and is not visible here`                                            | no |
| `S12` | error   | `partial assignment is permitted only on 'variable' identifiers`                                                    | no |
| `S14` | error   | `conditional expression requires an 'else' branch`                                                                  | yes |
| `S15` | error   | `bit-slice index must be a compile-time constant`                                                                   | no |
| `S16` | error   | `'param_int' / 'param_str' is meaningful only for Verilog/VHDL/SystemC submodules; pure-NSL modules use '#define'`  | no |
| `S17` | error   | `system task '<name>' is permitted only in modules whose 'declare' carries the 'simulation' modifier`               | no |
| `S20` | error   | `'interface' modifier requires explicit clock and reset signal names`                                               | no |
| `S21` | error   | `'finish' / 'invoke' is a built-in proc method; bare form is permitted only inside a 'proc' body`                   | no |
| `S21` | error   | `dotted form '<inst>.finish()' / '<inst>.invoke()' requires '<inst>' to resolve to a 'proc_name' declaration`       | no (mirrored) |
| `S22` | error   | `'return' may appear only inside a 'func' body`                                                                     | no |
| `S22` | error   | `return-expression width <N> does not match func's return-value-terminal width <M>`                                 | no (mirrored; `<N>` `<M>` are integer literals) |
| `S22` | error   | `bare 'return;' is valid only when the func has no return-value terminal`                                           | no (mirrored) |
| `S25` | error   | `'goto' inside a 'seq' block must target a label declared in the same block`                                        | no |
| `S25` | error   | `'goto' inside a 'state' body must target a 'state_name' declared in scope`                                         | no (mirrored) |
| `S26` | warning | `'function' is accepted as a synonym for 'func' but is not the canonical form`                                      | yes |
| `S28` | error   | `'first_state' must reference a 'state_name' declared in the enclosing 'proc'`                                       | no |
| `S28` | error   | `'first_state' may appear at most once per 'proc'`                                                                  | no (mirrored) |
| `S29` | error   | `'_init' block is permitted only at module top level inside a 'simulation'-modified module`                         | no |
| (resolution) | error | `unresolved name '<X>'`                                                                                       | no — emitted by `ResolutionPass`, not a constraint check |

**Total**: 32 frozen message rows covering 23 error/warning `Sn`
(some `Sn` have multiple frozen messages because the constraint has
multiple distinguishable failure modes). The unresolved-name
diagnostic from the `ResolutionPass` is not an `Sn` per se but is
included here because it is part of Sema's frozen surface (FR-017).

## Enforcement

- Each row's verbatim `<message>` is asserted by the corresponding
  `test/sema/s<NN>/fail.nsl` fixture's `// expected-error:` (or
  `// expected-warning:`) FileCheck directive.
- `S22` and `S25` etc. with multiple frozen messages get one
  `s<NN>/fail_<variant>.nsl` per variant (e.g.,
  `s22/fail_outside_func.nsl`, `s22/fail_width_mismatch.nsl`,
  `s22/fail_bare_with_return_terminal.nsl`).
- Renaming or weakening a frozen message is a spec amendment + a
  row update in this contract — never a routine PR.

## Fix-it hint shapes (FR-012)

| `Sn` | Fix-it | `replaceRange` | `replacement` |
|---|---|---|---|
| `S3` (`=` on reg) | replace `=` with `:=` | `TransferStmt::eqOpRange` | `:=` |
| `S3` (`:=` on wire) | replace `:=` with `=` | `TransferStmt::eqOpRange` | `=` |
| `S7` (when removable) | remove `seq` keyword + braces, re-flow contents inline | `SeqBlock::seqKwAndBracesRange` | `<stripped contents>` |
| `S14` (missing else) | insert ` else <expr>` template at conditional tail | `ConditionalExpr::endRange` (zero-width insert) | ` else ` (user fills `<expr>`) |
| `S26` (function→func) | replace `function` keyword | `function-keyword range` | `func` |

The fail fixture for each fix-it-bearing rule asserts both the
message text AND the `FixItHint` shape via FileCheck's `// expected-error: ... ; fix-it:` extension (per M1's diagnostic-format
plumbing). A regression on either is caught.

## What's NOT in this contract

- The 6 constructive `Sn` (`S13`, `S18`, `S19`, `S23`, `S24`,
  `S27`) emit no diagnostic; their test surface is the
  introspection API documented in `sema-api.contract.md`
  Invariant 4.
- The `unresolved name '<X>'` diagnostic message text is included
  above (last row) because it's part of Sema's frozen surface
  even though it's not a constraint check.
