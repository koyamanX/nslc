---
name: "nsl-test-author"
description: "Author per-Sn pass+fail tests, lit+FileCheck regression cases, per-rule lint fixtures, and generate golden VCDs — Principle VI's NON-NEGOTIABLE test discipline lives here."
argument-hint: "Test target (e.g., 'add S30 pass+fail tests')"
metadata:
  author: "nslc-project"
user-invocable: true
disable-model-invocation: false
---

## User Input

```text
$ARGUMENTS
```

You **MUST** consider the user input before proceeding (if not empty).

## Role

The test fixture author. Constitution **Principle VI is NON-NEGOTIABLE**: every library owns a corresponding test layer, and the project's canonical regression suite is the seven audited open-source NSL projects. Constitution **Principle VIII is NON-NEGOTIABLE**: the test MUST be written first, observed failing against the unchanged tree, and only then is the implementation accepted.

This skill covers all seven test layers:

| Layer | Driver | Constitution clause |
|---|---|---|
| Lexer | direct C++ | Principle VI bullet 1 |
| Parser | AST snapshot | Principle VI bullet 2 |
| Sema | one pass + one fail per `S1`–`S29`, `N1`–`N14` | Principle VI bullet 3 |
| Dialect | `nsl-opt` round-trip | Principle VI bullet 4 |
| Lowering | lit + FileCheck | Principle VI bullet 5 |
| End-to-end | audited corpus + Icarus / Verilator | Principle VI bullet 6 (NON-NEGOTIABLE) |
| Formal | riscv-formal | Principle VI bullet 7 (handled by `/nsl-formal`) |

## Outline

1. **Identify the layer and target.** Use `README.md` §Roadmap to find the milestone whose test gate this fixture belongs to. Use `docs/CLAUDE.md` §3 task-map to find the relevant spec/design sections.

2. **Write the test FIRST (Principle VIII rule).** The test MUST exist before the production code. The PR / commit history MUST show the test failing against the unchanged tree before the implementation commit lands. Squash-merge is OK if the PR description records the failing-state commit hash.

3. **Layer-specific authoring.**

   ### Lexer test
   - Asserts on token streams: keyword recognition, `_`-prefix system names, `%IDENT%` expansion, Z/X/U number-literal corner cases
   - Format: input `.nsl` snippet → expected token sequence

   ### Parser test
   - AST-snapshot test covering each grammar production from `docs/spec/nsl_lang.ebnf §§1–11`
   - Parser-note `N1`–`N14` disambiguation tests
   - Format: input `.nsl` → expected AST structure (snapshot-compared)

   ### Sema test (per `Sn`)
   - **Exactly one pass-case + one fail-case per `Sn`** (and per `Nn` where applicable)
   - **Fail-case MUST cite the specific diagnostic message string** the constraint produces. This catches downstream renaming / weakening of the diagnostic automatically.
   - File layout: `test/sema/S<n>/pass.nsl`, `test/sema/S<n>/fail.nsl`
   - Adding a new `Sn`/`Nn`/`Pn` requires adding both fixtures (Principle VI bullet 3, last sentence)

   ### Dialect test (M4)
   - `nsl-opt foo.mlir → verify → print → diff foo.expected.mlir` for every op listed in `docs/design/nsl_compiler_design.md` §7

   ### Lowering test (M5, M6)
   - lit + FileCheck on `nslc -emit=mlir` and `nslc -emit=hw`
   - Cover the per-op mapping from `docs/design/nsl_compiler_design.md` §10
   - Include a determinism check (two builds → byte-identical output)

   ### End-to-end test (M7, NON-NEGOTIABLE)
   - All seven audited projects (`cpu16`, `mips32_single_cycle`, `ahb_lite_nsl`, `mmcspi`, `SDRAM_Controler`, `rv32x_dev`, `turboV`) compile to Verilog and simulate equivalently to their `golden/*.vcd` under Icarus and Verilator
   - **Self-referential VCDs are forbidden** (Principle VI "Reference VCDs"): goldens MUST come from an external known-good source — upstream NSL toolchain output for non-CPU projects; manually-authored / formal-validated reference for CPU projects
   - Each project carries `test/audited/<project>/golden/REGEN.md` documenting the regeneration command
   - Vendoring (`PROVENANCE.md`) and golden-VCD generation are handled by `/nsl-driver-e2e` — coordinate with that skill

4. **Lint-rule fixtures (T6/T7).**
   - One passing case + one failing case + suggested-fix verification per rule
   - JSON-output schema test for `nsl-lint --format=json`
   - Hand-off to `/nsl-tooling-impl` for the rule implementation

5. **No-retrofit rule (Principle VIII).** Tests added in the same commit as the feature MUST still demonstrate failure: rebase or split commits so the failing-state is preserved in history. **Refactors** are exempt only when (a) test suite is fully green before AND after, (b) no new diagnostics, (c) no new IR ops or attributes, (d) no Verilog diff on the audited corpus.

6. **lit + FileCheck convention.** Per Principle VI: "lit + FileCheck convention from LLVM/CIRCT is the only accepted test driver for lowering and end-to-end tests." Do not introduce alternatives.

7. **Verify.** Confirm:
   - [ ] Test was authored BEFORE the implementation (commit history shows the failing state)
   - [ ] If `Sn` test: pass-case + fail-case both present; fail-case asserts on diagnostic string
   - [ ] If lint-rule fixture: pass + fail + fix-it + JSON-schema all present
   - [ ] If e2e: all seven audited projects still pass; no self-referential VCDs introduced
   - [ ] lit + FileCheck used (not an alternative driver)
   - [ ] SPDX header on every new fixture file

## Constitutional anchors

- **Principle V** — Inspectable, Deterministic Pipeline (determinism FileCheck cases at every `-emit=*`)
- **Principle VI** — Layered Test Discipline (NON-NEGOTIABLE)
- **Principle VIII** — Test-First Development (NON-NEGOTIABLE)
