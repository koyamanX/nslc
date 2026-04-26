---
name: "nsl-coupling-audit"
description: "Audit a PR or working tree for Principle VII spec ↔ design coupling: spec edits propagate to design docs, S/N/P quick-map stays current, line ranges in docs/CLAUDE.md §§4–7 are accurate."
argument-hint: "Optional PR number or scope hint (e.g., 'PR #42' or 'unstaged changes')"
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

Enforces Constitution **Principle VII** — Spec ↔ Design Coupling. The principle says:

> A change to `docs/spec/*.ebnf` MUST update the corresponding sections of `docs/design/*.md` in the same change, per the cross-reference table in `docs/CLAUDE.md` §8.

This skill is read-only by default — it produces a coupling-audit report. Repairs are deferred to `/nsl-spec-author` (spec side), the relevant implementation skill (design side), or a routine docs PR (TOC line-range fixes).

## When to invoke

- Before opening a PR that touches `docs/spec/`, `docs/design/`, or compiler/tooling code that contradicts a design doc
- When CodeRabbit flags possible spec/design drift
- Periodically (e.g., monthly) to catch silent drift across the tree
- After any large refactor that may have shifted section boundaries in spec or design files

## Outline

1. **Gather the diff.** Use `git diff <base>..HEAD` (default `<base>` = `main`) restricted to `docs/spec/`, `docs/design/`, `docs/CLAUDE.md`, root `CLAUDE.md`, and any code under `lib/`/`include/` that might contradict design.

2. **Audit the cross-reference table (`docs/CLAUDE.md` §8).** For each topic in the table, verify the listed spec ranges and design ranges still exist and still address the topic:
   - Lexical reserved words
   - `_`-prefix system names (N11)
   - `#line` directive (P13, N14)
   - Compile-time helpers (P5/P7)
   - `%IDENT%` macros (P3)
   - AST shape
   - Sema constraints S1–S29
   - Parser disambiguation N1–N14
   - `proc`/`state`/`finish` semantics (N6, S21)
   - `seq` / `while` / `for` placement (S7–S9)
   - `generate` unrolling (S10)
   - Hover / definition / refs
   - Lint rule W/S/H taxonomy

3. **Audit the S/N/P quick-map (`docs/CLAUDE.md` §5).** Every `Sn`, `Nn`, `Pn` declared in the EBNF MUST have a row. Every row MUST point at a still-valid line.

4. **Audit per-file TOC line ranges (`docs/CLAUDE.md` §§4–7).** Per Principle VII, line ranges in §§4–7 MUST be kept current when section boundaries in spec or design files shift. Verify with `grep -n '^## ' docs/spec/*.ebnf docs/design/*.md` (or use the convenience commands in `docs/CLAUDE.md` §12) and compare.

5. **Audit the language-feature roll-up in root `CLAUDE.md` §1.** When a new `Sn`/`Nn`/`Pn` was added, the row MUST be there (this is the audit hook). When a row is missing, flag a Principle VII violation.

6. **Audit the tooling-feature roll-up in root `CLAUDE.md` §2.** When a new LSP method, lint rule, formatter capability, highlighter scope, or editor target was added, the relevant sub-table row MUST be there.

7. **Audit milestone-plan coupling.** Per `CONTRIBUTING.md` §3.9, changes to delivery sequencing MUST update `README.md` §Roadmap and/or `CLAUDE.md` §1–§2. Flag any code change that looks like a delivery-sequencing change without matching milestone-plan updates.

8. **Produce the report.** Output in this format:

   ```
   ## Coupling-audit report — <date> / <scope>

   ### ✓ Clean
   - <topic>: <evidence>

   ### ✗ Violations (Principle VII)
   - <severity>: <topic> — <description>
     - Affected files: <paths>
     - Suggested fix: <delegation: route to /nsl-spec-author | /nsl-frontend-impl | docs PR>

   ### ⚠ Stale references
   - <topic>: <description>
     - Suggested fix: <docs PR>
   ```

9. **Do NOT auto-repair.** This skill flags; it does not fix. Repair work routes to:
   - **Spec-side fixes** → `/nsl-spec-author`
   - **Design-side fixes** → the relevant implementation skill (`/nsl-frontend-impl`, `/nsl-mlir-impl`, `/nsl-tooling-impl`, etc.)
   - **TOC line-range refresh** → routine docs PR
   - **Roll-up table row missing** → routine docs PR

## Constitutional anchors

- **Principle I** — Spec Is Authoritative (NON-NEGOTIABLE): if `docs/design/` and `docs/spec/` disagree, treat it as a bug in `docs/design/` and report it
- **Principle VII** — Spec ↔ Design Coupling: the rule this skill enforces

## Output discipline

- The report MUST cite specific file paths and line ranges; don't summarize abstractly
- The report MUST NOT propose code changes — only flag violations and suggest the routing skill
- "False positive — this is intentional" is a valid response from the user; record their rationale before closing the audit
