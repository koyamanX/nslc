---
name: "nsl-constitution-review"
description: "Review a PR or working tree against Constitution Principles V/VI/VII/VIII/IX (and the rest); produces a constitution-aware blocking-findings report that complements CodeRabbit (which is style/correctness-focused, not constitution-aware)."
argument-hint: "Optional PR number or scope (e.g., 'PR #42' or 'working tree')"
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

The constitution-aware reviewer. Constitution Principles I–IX define the project's hard invariants — but a generic code-review tool (CodeRabbit, `/ultrareview`) doesn't know them. This skill walks the constitution's principles against a diff and produces a blocking-findings report.

This skill is **read-only**. It produces findings and routes remediation to other skills. It is intended to run *alongside* CodeRabbit and `/ultrareview`, not instead of them.

## Authority and gating

Per Constitution **Merge gate composition** (External Integrations section):

> A PR is mergeable only when **all** of the following are true:
> 1. CI is green (Principle IX, or its transitional-clause equivalent)
> 2. CodeRabbit review has run and all blocking findings are addressed
> 3. Where applicable, the originating Linear issue is referenced

Constitutional findings — violations of any Principle — are **blocking** by definition. CodeRabbit findings are advisory **except** when they flag a constitution / spec / regression violation. This skill specifically catches the constitutional class.

## Outline

1. **Gather the diff.** `git diff <base>..HEAD`. Default `<base>` is `main`.

2. **Walk the principles.** For each principle, audit:

   ### Principle I — Spec Is Authoritative (NON-NEGOTIABLE)
   - Does the change to `lib/`/`include/`/`tools/` contradict `docs/spec/*.ebnf`? If so, this is a Principle I violation — the implementation must conform.
   - Is a new `Sn`/`Nn`/`Pn` reusing a retired number? Forbidden.

   ### Principle II — Layered Library Architecture
   - Does the change introduce an upward-layer dependency? (e.g., `nsl-lex` including `nsl-parse` headers.) Forbidden.
   - Does a tooling binary duplicate any of lex/parse/sema instead of using `libNSLFrontend.a`? Forbidden.

   ### Principle III — Stock CIRCT Below the `nsl` Dialect
   - Did the change introduce a hand-rolled netlist / register-inference / state-machine-lowering pass? Forbidden — file an upstream CIRCT issue instead.
   - Does Verilog emission go through `circt::ExportVerilog`? It must.

   ### Principle IV — Source-Locating Diagnostics
   - Do all new AST nodes / symbol-table entries / `nsl::*` ops carry a `SourceRange`?
   - Does a diagnostic render without a `file:line:col`? Bug.
   - Is `#line` directive metadata preserved through every later stage of the change?

   ### Principle V — Inspectable, Deterministic Pipeline
   - Does a new pipeline stage have its own `-emit=*` flag?
   - Does the change introduce non-determinism (pointer-address ordering, hash-map iteration, timestamps, env leakage)?
   - Verify byte-stability across two builds for any artifact the change touches (token stream, AST snapshot, MLIR, HW form, Verilog, golden VCD).

   ### Principle VI — Layered Test Discipline (NON-NEGOTIABLE)
   - Is the appropriate test layer green for this change? (Lexer / Parser / Sema / Dialect / Lowering / E2E / Formal)
   - For a new `Sn`/`Nn`/`Pn`: is there exactly one pass-case + one fail-case (with diagnostic-string assertion)?
   - For a new audited project or golden VCD: is the source external? Self-referential VCDs are forbidden.
   - Is `lit + FileCheck` used for lowering and e2e? It must be.

   ### Principle VII — Spec ↔ Design Coupling
   - Does the spec change have a matching design-doc update in the same PR?
   - Does the new `Sn`/`Nn`/`Pn` have a matching row in `docs/CLAUDE.md` §5 (quick-map) and root `CLAUDE.md` §1 (language-feature roll-up)?
   - Are line ranges in `docs/CLAUDE.md` §§4–7 still accurate?
   - For deeper coupling audit, route to `/nsl-coupling-audit`.

   ### Principle VIII — Test-First Development (NON-NEGOTIABLE)
   - Does PR history show the test failing against the unchanged tree before the implementation commit?
   - For a refactor: are all four exemption conditions met (a) test suite green before+after, (b) no new diagnostics, (c) no new IR ops or attributes, (d) no Verilog diff on the audited corpus?

   ### Principle IX — Continuous Integration & Delivery
   - Was a CI bypass used (`--no-verify`, `--no-gpg-sign`, etc.) without explicit user authorization recorded in the PR description?
   - Is the change a release? If so, are the artifacts CI-built (no human-built artifacts attached)?
   - If CI is offline, was the transitional-clause local-equivalent run, with output linked in the PR description?

3. **Check External Integrations gates.**
   - Is the originating Linear issue referenced (per the project's `nslc-<N>` convention)? Bugs go to GitHub Issues, not Linear.
   - Did CodeRabbit review run? If a blocking finding was overridden, is the rationale recorded?

4. **Produce the findings report.**

   ```
   ## Constitution review — <date> / <scope>

   ### ✓ Compliant
   - <principle>: <observation>

   ### ✗ Blocking findings
   - **Principle <N>** — <description>
     - File: <path:line>
     - Cite: <constitution clause>
     - Remediation: <delegation: route to /nsl-spec-author | /nsl-frontend-impl | /nsl-test-author | etc.>

   ### ⚠ Advisory
   - <description> (not blocking, but worth noting)
   ```

5. **Do NOT auto-repair.** Route findings to the appropriate skill:
   - Spec issue → `/nsl-spec-author`
   - Spec/design coupling → `/nsl-coupling-audit` then the relevant impl skill
   - Test missing → `/nsl-test-author`
   - Determinism → relevant impl skill (`/nsl-frontend-impl`, `/nsl-mlir-impl`, `/nsl-driver-e2e`)
   - Build/CI / no-bypass / SPDX → `/nsl-build-ci`
   - Constitutional ambiguity → run `/speckit-clarify` then possibly `/speckit-constitution`

## Authority on contradictions

If a finding contradicts a CodeRabbit suggestion: the constitution wins. CodeRabbit advisories may be declined with a brief rationale in the PR description. Constitutional findings (this skill) are blocking.

If a finding contradicts the constitution itself: that's a constitutional bug — file an issue, do not silently work around it. An amendment via `/speckit-constitution` may be the right answer.
