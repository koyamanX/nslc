<!-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception -->

# Specification Quality Checklist: M4 — `nsl` MLIR Dialect

**Purpose**: Validate specification completeness and quality before proceeding to planning
**Created**: 2026-04-30
**Feature**: [spec.md](../spec.md)

## Content Quality

- [x] No implementation details (languages, frameworks, APIs)
  - *Note*: The spec necessarily references TableGen, MLIR, and the
    `nsl-dialect` library — these are spec-level concerns (the
    Constitution mandates MLIR + TableGen as the IR framework per
    Principle III; the library set is fixed by Principle II). The
    spec does NOT prescribe internal class layouts, file partitions,
    or pass-implementation strategies.
- [x] Focused on user value and business needs
  - *Value framing*: M4 unlocks M5 (the AST→MLIR lowering) and is
    the dialect surface every downstream milestone consumes. The
    "user" at this layer is the M5/M6 implementor (and any
    contributor authoring hand-written `.mlir` test fixtures).
- [x] Written for non-technical stakeholders
  - *Caveat*: For an MLIR-dialect milestone the audience is
    inherently technical; the spec is written at the level a senior
    contributor familiar with MLIR conventions can follow without
    reading TableGen records.
- [x] All mandatory sections completed
  - User Scenarios & Testing, Requirements, Success Criteria — all
    present.

## Requirement Completeness

- [x] No [NEEDS CLARIFICATION] markers remain
  - *Resolved 2026-04-30*: Q1 (verifier strictness) → Option A
    (structural-only); Q2 (parent-relation semantics) → Option B
    (any-ancestor walk via custom verifier). Both folded into
    the Clarifications session, FR-011, FR-013, edge cases, and
    Assumptions. `grep "NEEDS CLARIFICATION"` returns zero
    matches.
- [x] Requirements are testable and unambiguous
  - Every FR has a paired test under `test/Dialect/` or a CI guard
    (FR-005, FR-021); SC-001 through SC-012 are mechanically
    verifiable.
- [x] Success criteria are measurable
  - SC-001 (35 round-trip fixtures); SC-002 (≥50 invalid fixtures);
    SC-005 (regex-matchable diagnostic format); SC-006 (byte-stable
    output across builds); SC-007 (byte-identical pre/post-M4 driver
    output); SC-011 (link-time dep graph). All quantified.
- [x] Success criteria are technology-agnostic (no implementation details)
  - *Caveat*: SC-003 names dialect types (`!nsl.bits<N>`, etc.) and
    SC-005 names the MLIR diagnostic format — these are
    spec-and-Constitution-level surfaces, not implementation details.
- [x] All acceptance scenarios are defined
  - US1 has 9 acceptance scenarios; US2 has 7; US3 has 4. Combined
    coverage: round-trip happy-path (US1), verifier-rejection
    sad-path (US2), driver invariant (US3).
- [x] Edge cases are identified
  - 13 edge cases, including degenerate types
    (`!nsl.bits<0>`, `!nsl.mem<[0 x ...]>`), empty constructs
    (zero-field struct, empty alt, empty proc), nesting violations
    (nested module), mixed-dialect input, and stdin mode.
- [x] Scope is clearly bounded
  - "What lands as a deliverable" and "What does NOT land at M4"
    blocks at the top of the spec are explicit; in-scope (35 ops +
    3 types + verifier + nsl-opt + test corpus) and out-of-scope
    (AST→MLIR, structural-expansion passes, CIRCT lowering,
    `-emit=mlir`, T-track, P-VEN/P-VCD, formal, release).
- [x] Dependencies and assumptions identified
  - Assumptions section enumerates: M0/M1/M2/M3 inheritance, dev
    container, MLIR/CIRCT pin source, tooling-track separation,
    constitutional layer rules.

## Feature Readiness

- [x] All functional requirements have clear acceptance criteria
  - FR-001 through FR-005: covered by SC-011 + dependency-graph
    guard. FR-006 through FR-008: covered by SC-003. FR-009 through
    FR-013: covered by SC-001 + SC-002. FR-014 through FR-016:
    covered by build smoke (`nsl-opt` runs). FR-017 through FR-021:
    covered by SC-001 + SC-002 + the per-PR TDD evidence rule.
    FR-022 through FR-024: covered by SC-007. FR-025 through
    FR-027: covered by SC-006. FR-028: covered by SC-010.
- [x] User scenarios cover primary flows
  - US1 (round-trip): the M4 acceptance gate.
  - US2 (verifier rejection): the structural-invariant gate.
  - US3 (driver invariant): the regression-guard.
- [x] Feature meets measurable outcomes defined in Success Criteria
  - SC-009 ("M4 unlocks M5") names the architectural payoff.
- [x] No implementation details leak into specification
  - *Caveat*: The FR-010 op table cites TableGen class names
    (`NSL_ModuleOp`, etc.) — these are referenced verbatim in
    design §7's TableGen excerpt, so they're spec-level, not
    implementation-leaked.

## Notes

- **Resolved 2026-04-30**: both clarifications closed in
  Clarifications session 2026-04-30. Q1 → Option A
  (structural-only) updated FR-011, FR-013 cardinality, edge
  cases for `nsl.alt` / `nsl.proc` / `nsl.connect`, and the
  `Per Q1 Option A` paragraph in Assumptions. Q2 → Option B
  (any-ancestor walk via custom verifier) updated FR-011's
  splitting of trait-declared vs hand-written verifier bodies,
  the FR-013 preamble note on row-by-row implementation style,
  and the `Per Q2 Option B` paragraph in Assumptions.
- All checklist items now pass; spec is ready for
  `/speckit-plan`.
