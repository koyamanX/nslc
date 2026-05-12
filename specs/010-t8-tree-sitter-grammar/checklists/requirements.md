<!-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception -->

# Specification Quality Checklist: T8 — Tree-sitter Grammar + Highlight Queries + VS Code WASM Consumer

**Purpose**: Validate specification completeness and quality before proceeding to planning
**Created**: 2026-05-05
**Feature**: [spec.md](../spec.md)

## Content Quality

- [x] No implementation details (languages, frameworks, APIs)
- [x] Focused on user value and business needs
- [x] Written for non-technical stakeholders
- [x] All mandatory sections completed

> Note on "no implementation details": the spec names tree-sitter,
> `grammar.js`, `queries/highlights.scm`, and `tree-sitter-nsl.wasm`
> because the **roadmap row** itself names them as the T8 deliverable
> ("Tree-sitter grammar (`grammar.js`) + highlight queries
> (`queries/highlights.scm`); VS Code extension shell consuming the
> WASM tree-sitter build" — `README.md` §Roadmap row T8). These are
> deliverable identifiers, not internal implementation details, and
> are required for the spec to anchor on the correct milestone.

## Requirement Completeness

- [x] No [NEEDS CLARIFICATION] markers remain
- [x] Requirements are testable and unambiguous
- [x] Success criteria are measurable
- [x] Success criteria are technology-agnostic (no implementation details)
- [x] All acceptance scenarios are defined
- [x] Edge cases are identified
- [x] Scope is clearly bounded
- [x] Dependencies and assumptions identified

> Note on "technology-agnostic success criteria": SC-002, SC-003,
> SC-005, SC-006, SC-008 are stated in terms of capture counts,
> error-node counts, byte-identity, and wallclock seconds — all
> measurable without naming a particular library. SC-007 mentions
> a folder-drop install path because that is the consumer-facing
> install model T1 already established and that T8 must honour.

## Feature Readiness

- [x] All functional requirements have clear acceptance criteria
- [x] User scenarios cover primary flows
- [x] Feature meets measurable outcomes defined in Success Criteria
- [x] No implementation details leak into specification

## Notes

- Items marked incomplete require spec updates before
  `/speckit-clarify` or `/speckit-plan`.
- **2026-05-05 clarifications session**: 4 high-impact
  ambiguities resolved (Q1 tree-sitter CLI version pin → minor
  version pin in CI + project config; Q2 WASM commit policy →
  don't commit, CI-uploaded workflow artefact + release attach;
  Q3 capture-name granularity → §4.3 set + 8 specific
  sub-captures; Q4 smoke-fixture source pre-P-VEN → in-tree
  `examples/*.nsl` corpus). All four are recorded in
  `## Clarifications` and integrated into FR-007/FR-008/FR-009/
  FR-010/FR-014, SC-001/SC-002/SC-003/SC-007, and the relevant
  Assumptions.
- Remaining plan-level decisions (not spec ambiguities): (a)
  whether to commit the generated `parser.c` to the repo (community
  default in tree-sitter ecosystem is yes; FR-017 already implies
  it via "running `tree-sitter generate` from the committed
  `grammar.js` produces no diff against the committed `parser.c`"),
  (b) which CI matrix cell hosts the T8 tests, (c) the exact
  pinned tree-sitter CLI minor version (e.g. `0.22.x` vs
  `0.23.x`). Reasonable defaults exist for all three.
- The spec relies on **T1's precedent** in three places: install
  model (folder-drop), no-binary-checkin norm (SC-005 of T1
  governs), and audited-corpus-availability fallback (T1 SC-001
  Assumption). All three are explicitly cross-referenced in the
  Assumptions section.
- The spec invokes **two scoped Constitution exceptions** that
  T1 also invoked: Principle II (no-duplication) — the
  highlighter tier is permitted a parallel parser; Principle V
  (determinism) — applied to the WASM build artefact.
