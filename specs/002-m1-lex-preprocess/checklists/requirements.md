<!-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception -->

# Specification Quality Checklist: M1 — Lex + Preprocess (with Diagnostic Plumbing)

**Purpose**: Validate specification completeness and quality before proceeding to planning
**Created**: 2026-04-27
**Feature**: [spec.md](../spec.md)

## Content Quality

- [x] No implementation details (languages, frameworks, APIs)
  - Note: language scope (C++17) and library names (`nsl-basic`,
    `nsl-lex`, `nsl-preprocess`) are unavoidable at this layer
    because they are constitutional and Roadmap-fixed (not free
    variables). Spec defers all *implementation* choices below
    layer/header level to plan.md per spec-template policy.
- [x] Focused on user value and business needs
- [x] Written for non-technical stakeholders
  - As far as a compiler-internals milestone permits; "user" here
    is the next-layer contributor (M2 parser author), per the
    project's standard interpretation.
- [x] All mandatory sections completed

## Requirement Completeness

- [x] No [NEEDS CLARIFICATION] markers remain
- [x] Requirements are testable and unambiguous
- [x] Success criteria are measurable
- [x] Success criteria are technology-agnostic (no implementation details)
- [x] All acceptance scenarios are defined
- [x] Edge cases are identified
- [x] Scope is clearly bounded
- [x] Dependencies and assumptions identified

## Feature Readiness

- [x] All functional requirements have clear acceptance criteria
- [x] User scenarios cover primary flows
- [x] Feature meets measurable outcomes defined in Success Criteria
- [x] No implementation details leak into specification

## Notes

- Items marked incomplete require spec updates before
  `/speckit-clarify` or `/speckit-plan`.
- /speckit-clarify session 2026-04-27 resolved 3 questions
  (compile-time helper eval locus; JSON-diagnostic depth at M1;
  `nslc -emit=tokens` driver inclusion at M1) — see the
  `## Clarifications` section in spec.md. The CLAUDE.md §1 row on
  helper evaluation is now flagged as a Principle-VII coupling-fix
  follow-up patch (separate PR), not a spec-text issue.
- The N5 lexer-classification interpretation (CLAUDE.md §1 parser-
  notes row "M2 (incl. N5 ...)") remains documented as an
  assumption with rationale; not asked in this clarify session
  because the rationale is internal-consistent (lexer must classify
  `#` correctly when emitting a token; parser at M2 consumes the
  classification). Promote to a clarify question only if a future
  reader contests the interpretation.
- The compile-time-helper closed set (22 identifiers, verified
  against `nsl_pp.ebnf §3` lines 282–288 during /speckit-clarify
  scan) is reproduced verbatim in the spec's Assumptions paragraph
  and in FR-017. The M1 implementation tracks it via a single
  source-of-truth header so any future EBNF widening of the set is
  a one-line spec patch.
- All P-notes P1–P13 are covered in FR-034. Notes that express
  aggregate seam invariants (P12) rather than discrete violatable
  rules ship pass-only and are exercised indirectly via the
  contributing P-notes (P6, P7, etc.) — this is documented in
  FR-034 itself.
