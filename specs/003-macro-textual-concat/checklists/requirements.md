<!-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception -->

# Specification Quality Checklist: Bare-Macro Textual Concatenation

**Purpose**: Validate specification completeness and quality before proceeding to planning
**Created**: 2026-04-27
**Feature**: [spec.md](../spec.md)

## Content Quality

- [x] No implementation details (languages, frameworks, APIs)
  - Note: the FR-001 / FR-008 spec-amendment text references `pp.ebnf`
    P10 by name; that's the spec being amended, not an implementation
    detail. Implementation file references in FR-003+ live under
    "Implementation deltas" — appropriate at this layer because the
    feature *is* a behavior change in `lib/Preprocess/`.
- [x] Focused on user value and business needs
  - User: NSL preprocessor author. Value: write the canonical
    pp.ebnf P5 example without workarounds.
- [x] Written for non-technical stakeholders
  - As far as a preprocessor-internals feature permits.
- [x] All mandatory sections completed

## Requirement Completeness

- [x] No [NEEDS CLARIFICATION] markers remain
- [x] Requirements are testable and unambiguous
- [x] Success criteria are measurable
- [x] Success criteria are technology-agnostic (no implementation details)
  - Exception: SC-006 references "pp.ebnf line count ±2" — that's
    a constitutional drift-surface metric, technology-anchored to the
    `.ebnf` file format. Acceptable per the existing precedent in M1.
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

- This spec deliberately bundles BOTH a `pp.ebnf` P10 amendment
  (FR-001/FR-002) AND an implementation change (FR-003+) into a
  single feature. Per Constitution Principle VII (spec/design
  coupling) the two MUST land together — splitting them would
  create a transitional state where the spec says one thing and
  the implementation does another.
- Three reasonable defaults documented in Assumptions (substitution
  scope, adjacency rules, recursion budget). All have grounded
  rationales (pp.ebnf P2/P3, M1 FR-022). If any is contested at
  /speckit-clarify time the spec needs an amendment, but baseline
  defaults are sound.
- M1's two workaround fixtures (`p10/pass.test` and `p12/pass.test`)
  can be reverted to the canonical form as a clean-up indicator
  (FR-014, SC-002). Optional — the feature works either way.
