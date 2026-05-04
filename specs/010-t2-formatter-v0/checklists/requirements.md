# Specification Quality Checklist: T2 — Formatter v0 (`nsl-fmt`)

**Purpose**: Validate specification completeness and quality before proceeding to planning
**Created**: 2026-05-04
**Feature**: [spec.md](../spec.md)

## Content Quality

- [x] No implementation details (languages, frameworks, APIs)<br>*Wadler–Leijen / Doc IR / `libNslFmt.a` are named because the design doc itself names them; the spec describes user-visible behavior, not C++ class skeletons.*
- [x] Focused on user value and business needs
- [x] Written for non-technical stakeholders<br>*A project lead, CI maintainer, or NSL author can read and understand the four user stories without compiler internals.*
- [x] All mandatory sections completed (User Scenarios, Requirements, Success Criteria)

## Requirement Completeness

- [x] No [NEEDS CLARIFICATION] markers remain<br>*All three clarifications resolved by `/speckit-clarify` session 2026-05-04: Q1 (preprocessor scope) → Option A (raw-input parse + opaque directive tokens, clang-format style); Q2 (`--range` scope) → Option A (ship at T2); Q3 (multi-file failure handling) → Option A (continue-on-error, gofmt/black style). Recorded in `## Clarifications` section of the spec; FR-003a, FR-007, FR-012a, FR-018 updated; Key Entities and Dependencies sections updated; Open Questions section removed.*
- [x] Requirements are testable and unambiguous<br>*Every FR-### is observable through CLI behavior, exit code, or build output.*
- [x] Success criteria are measurable<br>*SC-002 (idempotence on 7 audited files), SC-003 (250 ms / 1000-line file), SC-005 (~30 lines of glue at T5) are concrete metrics.*
- [x] Success criteria are technology-agnostic (no implementation details)
- [x] All acceptance scenarios are defined<br>*4 user stories × 2–5 Given/When/Then scenarios each.*
- [x] Edge cases are identified<br>*Six edge cases listed: parse error, preprocessor directives, over-long lines, empty input, mixed line endings, BOM.*
- [x] Scope is clearly bounded<br>*Explicit OUT-of-scope list: LSP integration, pre-commit hook, daemon mode, non-`.nsl` files, `// nsl-fmt: off` islands.*
- [x] Dependencies and assumptions identified<br>*Assumptions section enumerates 8 reasonable defaults + 5 scope boundaries + 4 dependencies.*

## Feature Readiness

- [x] All functional requirements have clear acceptance criteria<br>*FR-001..FR-007 → User Story 1/2; FR-008..FR-012 → User Story 1; FR-013..FR-016 → User Story 4; FR-017..FR-019 → User Story 3; FR-020..FR-022 → Principle VI test discipline.*
- [x] User scenarios cover primary flows<br>*P1 author flow, P2 CI flow, P3 LSP-link flow, P3 config flow.*
- [x] Feature meets measurable outcomes defined in Success Criteria<br>*Each SC traces to one or more FRs (e.g., SC-002 ↔ FR-008, SC-003 ↔ FR-003).*
- [x] No implementation details leak into specification

## Notes

- Iteration count: 2 (initial draft + post-`/speckit-clarify`
  integration). All 13 checklist items now pass.
- The "no implementation details" check passes by interpretation:
  named entities (CST, Doc IR, `libNslFmt.a`) are reused vocabulary
  from the existing design doc, not new architectural decisions
  introduced by this spec. The spec describes *what they accomplish
  for the user*, not *how they are coded*.
- The directive-aware pre-pass introduced by Q1 is the largest
  scope amendment from the original draft: it requires a new
  parsing path that operates on raw (pre-preprocessing) source.
  FR-018 explicitly preserves Principle II (no-duplication) by
  routing inter-directive NSL fragments through the existing
  `libNSLFrontend.a` parser (extended only with a CST-emitting
  mode), so the pre-pass is the only net-new parsing code T2 owns.
  This is a load-bearing decision for `/speckit-plan`: any plan
  that hand-rolls a second NSL parser violates Principle II and
  this spec.
