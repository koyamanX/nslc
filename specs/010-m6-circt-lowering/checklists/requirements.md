<!-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception -->

# Specification Quality Checklist: M6 — `nsl` → CIRCT lowering

**Purpose**: Validate specification completeness and quality before proceeding to planning
**Created**: 2026-05-04
**Feature**: [spec.md](../spec.md)

## Content Quality

- [x] No implementation details (languages, frameworks, APIs)
  - *Note*: The spec necessarily names CIRCT dialects (`hw`, `comb`, `seq`, `fsm`, `sv`, `hwarith`) and MLIR types because they are the WHAT of M6, not the HOW. This matches the M5 spec's pattern (which named the `nsl` dialect ops it was producing). Library-internal class shapes (the `TypeConverter`, individual `ConversionPattern` subclasses) are mentioned as Key Entities, not as implementation guidance — the planner is free to subdivide them.
- [x] Focused on user value and business needs (the contributor running `nslc -emit=hw` and the downstream M7 pipeline)
- [x] Written for a project-internal audience (matches M5/M4 spec tone — "non-technical stakeholders" for a compiler-internals milestone is interpreted as "contributor onboarding the milestone scope")
- [x] All mandatory sections completed (User Scenarios & Testing, Requirements, Success Criteria)

## Requirement Completeness

- [x] No [NEEDS CLARIFICATION] markers remain
  - **Resolved at specify-time (2026-05-04)**: Q1 → Option B (`_init` lowers to `sv.initial { … }` under `sv.ifdef "SIMULATION"`); Q2 → Option A (`-emit=hw` halts strictly at the nsl→CIRCT boundary). Edits applied in-place to FR-019, FR-027, Edge Cases, and Assumptions.
- [x] Requirements are testable and unambiguous (each FR has a measurable observable: pattern registered, op produced, diagnostic emitted, fixture passes)
- [x] Success criteria are measurable (SC-001 wall-clock budget, SC-002 100% coverage, SC-003 byte-identical output, SC-004 zero `UnknownLoc`, SC-005 amendment workflow, SC-006 standalone equivalence, SC-007 link-library count cap, SC-008 constitutional anchor list)
- [x] Success criteria are technology-agnostic at the *outcome* level even where the spec names dialects (the criterion "zero ops in `nsl` dialect" is verifiable without specifying HOW the conversion runs)
- [x] All acceptance scenarios are defined (US1: 6 scenarios; US2: 5; US3: 5; US4: 7; US5: 4)
- [x] Edge cases are identified (`_init` target, post-conversion CIRCT-pass invocation, fail-fast on M5 bug, empty module, sim-only module, symbol collision, width mismatch fall-through)
- [x] Scope is clearly bounded (the "What does NOT land at M6" prologue paragraph plus FR-031–FR-034)
- [x] Dependencies and assumptions identified (Assumptions section enumerates M5 stability, M4 ABI freeze, vendored CIRCT availability, audited-corpus M7-deferral, determinism inheritance, sv-coverage assumption, two outstanding clarifications, comb-vs-hwarith planner choice)

## Feature Readiness

- [x] All functional requirements have clear acceptance criteria (each FR pairs to a US scenario or SC measurable outcome)
- [x] User scenarios cover primary flows (US1 acceptance gate, US2 module skeleton, US3 FSM, US4 leaf-ops, US5 round-trip + determinism)
- [x] Feature meets measurable outcomes defined in Success Criteria (SC-001 through SC-008 are independently verifiable)
- [x] No implementation details leak into specification beyond the design-§10 mapping table references (which are the WHAT — see Content Quality note above)

## Notes

- **All clarifications resolved at specify-time (2026-05-04)**: Q1 → B, Q2 → A. No markers remain. The spec is closed and ready for `/speckit-clarify` (which may surface deeper questions beyond the two specify-time markers — e.g., `comb.*` vs `hwarith.*` op-family choice, `TypeConverter` struct-packing edge cases, conversion-pattern ordering when multiple patterns target the same op kind).
- **No further iteration needed at the spec level**.
- **Style alignment with M5 spec is intentional**. The "Scope interpretation" prologue, the "What lands / What does NOT land" paragraph pair, the Constitutional anchors in Why-this-priority paragraphs, and the per-US Independent Test paragraphs all match `specs/008-m5-structural-passes/spec.md`'s structure. This is the project-canonical milestone-spec style; deviating would create spec-style drift across M-track milestones.
