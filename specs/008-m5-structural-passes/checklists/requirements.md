<!-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception -->

# Specification Quality Checklist: M5 — `nsl-lower` part 1

**Purpose**: Validate specification completeness and quality before proceeding to planning
**Created**: 2026-04-30
**Feature**: [spec.md](../spec.md)

## Content Quality

- [x] No implementation details (languages, frameworks, APIs)
- [x] Focused on user value and business needs
- [x] Written for non-technical stakeholders
- [x] All mandatory sections completed

> **Domain caveat (mirrors M4 checklist precedent)**: This is a compiler-engineering
> milestone whose stakeholders ARE compiler engineers. The spec necessarily names
> MLIR / `nsl::*` op types, AST node kinds, and pass identifiers because those
> are the user-facing observables of the library being built. The "no
> implementation details" item passes in the sense that the spec does not
> dictate INTERNAL implementation choices (private headers, internal helper
> classes, `mlir::PassManager` ordering knobs beyond the public order, struct
> layouts, naming conventions for private symbols) — those are deferred to
> `plan.md`. The PUBLIC entities named here (the visitor, the six passes, the
> `-emit=mlir` flag, the umbrella header) are the spec's deliverable surface;
> they are stakeholder-visible by definition.

## Requirement Completeness

- [x] No [NEEDS CLARIFICATION] markers remain
- [x] Requirements are testable and unambiguous
- [x] Success criteria are measurable
- [x] Success criteria are technology-agnostic (no implementation details)
- [x] All acceptance scenarios are defined
- [x] Edge cases are identified
- [x] Scope is clearly bounded
- [x] Dependencies and assumptions identified

> **SC technology-agnostic check (mirrors M4 checklist precedent)**: The
> success-criteria items name `nsl::*` ops, `mlir::ModuleOp`, `nsl-opt`, the
> `-emit=mlir` flag, and similar entities. These are the user-observable
> surface of the deliverable. They are NOT framework choices ("React",
> "PostgreSQL", "Redis") — the library being built IS an MLIR dialect lowering;
> naming MLIR is naming the project, not naming an implementation tool. The
> SC items remain measurable, verifiable, and outcome-focused (cardinality
> equalities, byte-identical diffs, exit codes), which is the substance of the
> "technology-agnostic" rule. M4's checklist passed under the same precedent.

## Feature Readiness

- [x] All functional requirements have clear acceptance criteria
- [x] User scenarios cover primary flows
- [x] Feature meets measurable outcomes defined in Success Criteria
- [x] No implementation details leak into specification

## Notes

- All five user stories (US1 P1 — `-emit=mlir` end-to-end; US2 P1 — generate-unroll; US3 P2 — variable-to-SSA; US4 P2 — `%IDENT%` residue check; US5 P3 — determinism) trace cleanly to the README's M5 row test gate ("FileCheck on `nslc -emit=mlir` for representative samples per AST node kind; determinism gate (byte-stable across two builds)") plus the parenthetical sub-deliverables ("generate-loop unroll, struct-SSA-split, `%IDENT%` residue check") plus design §9's six-pass list.
- The 31 functional requirements are organised in seven sub-sections: library scaffolding (FR-001–003), visitor (FR-004–010), pass pipeline (FR-011–019), driver wiring (FR-020–024), determinism (FR-025–026), testing (FR-027–030), and spec coupling (FR-031). Each sub-section maps cleanly to one of the five user stories or to a constitutional principle anchor.
- The 12 measurable success criteria (SC-001–012) collectively cover: visitor coverage (SC-001), pass surface (SC-002), corpus pass-rate (SC-003, SC-010), per-pass shape (SC-004, SC-005, SC-006), determinism (SC-007, SC-008), source-locating discipline (SC-009), constitutional gate (SC-011), and forward-compatibility (SC-012).
- 10 edge cases are catalogued covering empty input, generate-with-zero-bound, variable-without-write, submod-array-with-one-element, `func_self`-noninlinable, diagnostic-flood, mid-pipeline-failure, write-failure, and stdin-piped input.
- The Assumptions section locks in the interpretation of the README's "representative samples per AST node kind" gate (one fixture per `visit()` override), the `NSLCheckSemanticsPass` Sn-subset (implementer-determined at plan time), and the `NSLInlineInternalFuncPass` no-op-permitted clause. These resolve the three questions that surfaced during drafting and would otherwise have been [NEEDS CLARIFICATION] markers; they are documented as informed-default assumptions per the `/speckit-specify` workflow's "make informed guesses" rule.
- Items marked incomplete require spec updates before `/speckit-clarify` or `/speckit-plan`. All items currently pass; `/speckit-clarify` is OPTIONAL on this spec but RECOMMENDED — the M4 spec ran six clarification rounds and surfaced two API contract gaps that would have blocked implementation; M5's higher-stakes pipeline (six passes touching every IR shape from M4) will likely surface analogous gaps. Plan to budget a clarification session.
