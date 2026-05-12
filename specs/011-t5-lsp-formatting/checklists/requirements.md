<!-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception -->

# Specification Quality Checklist: T5 — LSP Formatting Integration

**Purpose**: Validate specification completeness and quality before proceeding to planning
**Created**: 2026-05-12
**Feature**: [spec.md](../spec.md)

## Content Quality

- [x] No implementation details (languages, frameworks, APIs)
  *(Spec references the frozen T2 `Fmt.h` API and the T3 `lsp-protocol.contract.md` because they are the contractual surface this milestone consumes, not because they are implementation details internal to T5. NSL-specific identifiers like `nsl::fmt::format_buffer` appear only where they name an external boundary the requirements must agree with — same vocabulary the audited T2 and T3 specs already use.)*
- [x] Focused on user value and business needs
- [x] Written for non-technical stakeholders
  *(LSP protocol terms are unavoidable for a milestone whose deliverable IS two LSP methods; the user-story sections frame the value in editor-author terms.)*
- [x] All mandatory sections completed (User Scenarios, Requirements, Success Criteria, Assumptions)

## Requirement Completeness

- [x] No [NEEDS CLARIFICATION] markers remain
  *(All 3 markers resolved in `/speckit-clarify` Session 2026-05-12: Q1 FR-005 → TOML wins (Option A); Q2 FR-006 → single whole-span TextEdit (Option A); Q3 FR-007 → return null on refusal (Option A). Two additional clarifications added in the same session: Q4 FR-005c → malformed TOML falls back to defaults + diagnostic; Q5 FR-014b → in-flight format completes through `didClose`.)*
- [x] Requirements are testable and unambiguous
- [x] Success criteria are measurable
- [x] Success criteria are technology-agnostic
  *(SC-001 through SC-011 measure byte-equivalence with the CLI, response latency, deterministic output, structural Principle II compliance, and CI runtime. No SC mentions a framework or library by name except where the library IS the contractual boundary, e.g., SC-007's linker-map assertion against `libNslFmt.a`.)*
- [x] All acceptance scenarios are defined
- [x] Edge cases are identified
  *(17 edge cases listed: parse-error, Sema-error, empty document, directives-only, range overlapping directive, range covering whole document, inverted range, concurrent edit, cancellation during format, FormattingOptions interaction, missing TOML, malformed TOML (added Session 2026-05-12), non-file URI scheme, full-sync buffer reuse, timing & cancellation poll interleave, willSaveWaitUntil non-advertisement, didClose during format (added Session 2026-05-12), TextEdit shape on success.)*
- [x] Scope is clearly bounded
  *(Six explicit "T5 does NOT" requirements (FR-022 to FR-025), plus the "Roadmap anchor" section's deferral table, plus the "Scope boundaries" assumption block.)*
- [x] Dependencies and assumptions identified
  *(Two prerequisite-delivered blocks: T2 and T3. Seven reasonable-default blocks. Three explicit-dependency blocks.)*

## Feature Readiness

- [x] All functional requirements have clear acceptance criteria
- [x] User scenarios cover primary flows
  *(Four user stories: P1 whole-document formatting, P2 range formatting, P2 format-on-save CLI parity, P3 architectural seam.)*
- [x] Feature meets measurable outcomes defined in Success Criteria
- [x] No implementation details leak into specification
  *(Source-file names like `lib/LSP/Features/Formatting.cpp` appear only where the design-doc directory-layout convention forces a specific file location — the file path is the boundary the seam claim FR-010 asserts against.)*

## Notes

- All checklist items pass as of `/speckit-clarify` Session 2026-05-12.
- The three original [NEEDS CLARIFICATION] markers (FR-005, FR-006, FR-007) were the deliberate output of step 5.3 of the specify workflow — only the highest-impact decisions that had multiple reasonable interpretations. All three resolved to their recommended Option A (TOML wins / single whole-span TextEdit / return null on refusal). The two additional clarifications added in the same session (Q4 malformed TOML → FR-005c; Q5 mid-format didClose → FR-014b) tightened previously-implicit edge cases.
- Cross-reference back-edges: Every spec-cross-reference table in the T2 and T3 contracts that pointed forward to "T5" (T2 spec FR-019, T2 SC-005, T3 FR-019, T3 spec "format-region seam" mention) is now closed by a forward-edge from this spec to those anchors. No dangling forward references remain.
- Ready for `/speckit-plan`.
