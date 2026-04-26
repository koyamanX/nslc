---
name: "nsl-spec-author"
description: "Edit docs/spec/*.ebnf, add/clarify semantic constraints (Sn), parser notes (Nn), and preprocessor notes (Pn) — cross-checking audited NSL projects as ground truth."
argument-hint: "Spec change or new Sn/Nn/Pn description"
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

The spec author is the steward of `docs/spec/*.ebnf` — Constitution Principle I declares this directory the sole authoritative source for **what NSL is**. The compiler implementation in `docs/design/*.md` and code in `lib/` are subordinate.

Use this skill when:
- Adding a new semantic constraint (`S30`, `S31`, …), parser note (`N15`, …), or preprocessor note (`P14`, …)
- Clarifying an existing `Sn`/`Nn`/`Pn` (e.g., from a `/speckit-clarify` answer)
- Resolving a spec/PDF disagreement (audited NSL projects are the tiebreaker; record the interpretation in the relevant `.ebnf` header)
- Adding a grammar production or modifying an existing one

## Outline

1. **Locate the change.** Use [`docs/CLAUDE.md`](../../../docs/CLAUDE.md) §5 (S/N quick-map) or §3 (task → section map) to find the right line range. Read only the affected sections.

2. **Apply Principle I monotonic numbering.** When adding `Sn`/`Nn`/`Pn`, append the next free integer. **Never reuse a retired number.**

3. **Edit the EBNF.**
   - `docs/spec/nsl_lang.ebnf` for grammar / `Sn` / `Nn`
   - `docs/spec/nsl_pp.ebnf` for preprocessor / `Pn`
   - Note the change in the file's header comment with date

4. **Cross-check the change against audited NSL projects.** When the PDF reference manuals disagree, the audited corpus (`cpu16`, `mips32_single_cycle`, `ahb_lite_nsl`, `mmcspi`, `SDRAM_Controler`, `rv32x_dev`, `turboV`) is the ground truth. Record the interpretation rationale in the `.ebnf` header comment.

5. **Propagate per Principle VII (NON-NEGOTIABLE coupling).** A spec change MUST update in the same PR:
   - `docs/design/nsl_compiler_design.md` sections per the cross-reference table in `docs/CLAUDE.md` §8
   - `docs/CLAUDE.md` §5 (S/N quick-map) — add/move a row for the new marker
   - `docs/CLAUDE.md` §§4–7 (per-file TOCs) — refresh line ranges if section boundaries shifted
   - **`CLAUDE.md` (project root) §1** — language-feature roll-up MUST gain a row for the new `Sn`/`Nn`/`Pn` (this is the audit hook)

6. **Schedule the test (Principle VIII — TDD, NON-NEGOTIABLE).** Hand off to `/nsl-test-author` to:
   - Write a pass-case test (constraint satisfied)
   - Write a fail-case test that asserts on the **specific diagnostic string** the constraint produces
   - Observe both failing against the unchanged tree before any implementation lands
   - Tests target the M3 sema layer (`Sn`), M2 parser (`Nn` mostly), or M1 preprocessor (`Pn`)

7. **Verify.** Confirm:
   - [ ] Spec change recorded in `.ebnf` header with date + rationale
   - [ ] `docs/design/*.md` updated per cross-reference table
   - [ ] `docs/CLAUDE.md` §5 quick-map row added/updated
   - [ ] `docs/CLAUDE.md` §§4–7 line ranges still accurate
   - [ ] `CLAUDE.md` §1 language-feature roll-up row added (for `Sn`/`Nn`/`Pn`)
   - [ ] Pass-case + fail-case tests written and observed failing
   - [ ] No upstream NSL PDF files were committed

## Constitutional anchors

- **Principle I** — Spec Is Authoritative (NON-NEGOTIABLE)
- **Principle VII** — Spec ↔ Design Coupling
- **Principle VIII** — Test-First Development (NON-NEGOTIABLE)

## Authority on contradictions

If `docs/spec/` and `docs/design/` disagree → bug in `docs/design/`, file an issue. If `docs/spec/` and an upstream PDF disagree → record the deliberate interpretation in the `.ebnf` header (audited corpus wins). See `docs/CLAUDE.md` §10–§11.
