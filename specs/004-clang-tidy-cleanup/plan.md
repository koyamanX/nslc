<!-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception -->
# Implementation Plan: clang-tidy Cleanup — Retire CI Static-Checks Debt

**Branch**: `004-clang-tidy-cleanup` | **Date**: 2026-04-27 | **Spec**: [spec.md](./spec.md)
**Input**: Feature specification from `/specs/004-clang-tidy-cleanup/spec.md`

## Summary

Drive the CI `static-checks` stage from 927 warnings-treated-as-errors down to
0 across the existing M0/M1/M3 source tree, retire Constitution Principle IX's
transitional clause once the gate is durably green, and put a regression-
prevention mechanism in place so the gate stays green going forward.

The work splits into two natural phases — **fix-or-suppress** (per-category
commits that bring counts to zero) and **lock-in** (constitution edit + CI
config tightening). Per memory note `feedback_clang_tidy_batch_unsafe.md`,
batch `--fix` is documented to break the build; the plan deliberately
sequences each category as a discrete commit so the tree remains buildable
at every step and a bisect across the feature branch lands on a buildable
HEAD at every commit (SC-005).

## Technical Context

**Language/Version**: C++17 (matches existing M0/M1/M3 source per
`CONTRIBUTING.md` §3.1).
**Primary Dependencies**: clang-tidy 18 (pinned in
`ghcr.io/koyamanx/nsl-nslc:dev`), clang-format 18 (same image),
`scripts/check_spdx.py` (in-tree).
**Storage**: N/A — pure source-tree edits.
**Testing**: Existing layered test discipline (lit + ctest) per
Principle VI; no new test artifacts authored by this feature, but every
per-category commit MUST run the full lit + ctest suite green before
landing.
**Target Platform**: Build environment is the canonical
`ghcr.io/koyamanx/nsl-nslc:dev` container (per memory note
`project_build_environment.md`); host toolchains may differ.
**Project Type**: Compiler/CLI (existing nslc tree).
**Performance Goals**: N/A for the cleanup itself. The CI `static-checks`
stage already runs in under 5 minutes on a clean tree; the cleanup MUST
not measurably increase that time (config tightening only).
**Constraints**: Public-header API surface frozen (FR-010). SPDX
preservation (FR-008). No TODO/FIXME workarounds (FR-009). Per-category
commit discipline (FR-004). Locked diagnostic strings unchanged (FR-007).
**Scale/Scope**: Source tree at master HEAD `73e49ae`: ~286 SPDX-checked
files; the static-checks stage names ~13 high-volume warning categories
covering 916 of the 927 errors (the remaining ~11 are clang-format
violations and tail-end categories).

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-check after Phase 1 design.*

| Principle | Applies? | Status | Notes |
|---|---|---|---|
| **I. Spec Is Authoritative** | Forward-looking | ✅ | No grammar/spec change. The cleanup is style/correctness only on the implementation tree; no `.ebnf` edits, no `Sn`/`Nn`/`Pn` numbering. |
| **II. Layered Library Architecture** | Yes | ✅ | All edits stay within their existing layer. `tools/nslc/main.cpp` 60-line cap preserved (the 5 readability-implicit-bool-conversion warnings there are clang-format/`!std::strcmp(...)`-style, not size-changing). No new layers, no layer crossings. |
| **III. Stock CIRCT Below `nsl` Dialect** | Forward-looking | ✅ | No dialect or CIRCT-adjacent code touched. |
| **IV. Source-Locating Diagnostics** | Yes | ✅ | M1's `DiagnosticEngine` and `SourceLocation` API not touched in any signature-changing way. The 22 `misc-non-private-member-variables-in-classes` hits include `Diagnostic` POD struct fields exposed by `diagnostics()` — that public API stays. |
| **V. Inspectable, Deterministic Pipeline** | Yes | ✅ | No env-var or hash-derived ordering introduced. The cleanup MUST not change observable lexer/parser/preprocessor behavior; lit + ctest determinism fixtures continue to pass byte-identically per FR-007 / SC-003. |
| **VI. Layered Test Discipline** | Yes — gating | ✅ | Existing lit (118) + ctest (129) suites are the steady-state regression set. No new fixtures authored by this feature; the cleanup is "make existing tree clean" not "add behavior." Each per-category commit MUST run the full suite green. |
| **VII. Spec ↔ Design Coupling** | Yes — gating | ✅ | No spec change, no design-doc change to the M1 layer architecture. The `.clang-tidy` config is project-policy artifact, not a design doc; updates land in this PR alongside the cleanup, not as a separate doc PR. |
| **VIII. Test-First Development** | **Yes — NON-NEGOTIABLE** | ✅ | TDD interpretation: a "test" for the cleanup is a green CI run. Each per-category commit's RED-state evidence is the warnings-treated-as-errors count visible in the previous commit's CI output; the GREEN-state evidence is the count drop in this commit's CI output. The commit-message body cites both per FR-011. |
| **IX. Continuous Integration & Delivery** | **Yes — load-bearing, AND retired** | ✅ | This feature is THE feature that retires Principle IX's transitional clause. Pre-cleanup state: transitional clause active (red gate tolerated). Post-cleanup state: steady-state Principle IX rule (green gate mandated). The transitional-clause edit is a separate close-out commit per SC-004. |
| **Build/Code/Licensing Standards** | Yes | ✅ | C++17 unchanged. SPDX header preservation per FR-008. LLVM/CIRCT conventions match: any `[[nodiscard]]` adoption (47 sites) follows the LLVM coding standard. |
| **Development Workflow** | Yes | ✅ | Spec Kit pipeline: `/speckit-specify` → `/speckit-plan` (this commit) → `/speckit-tasks` → `/speckit-implement`. Per-category cleanup commits follow CONTRIBUTING §5 squash-merge guidance (the category-by-category history is preserved in the merge commit body). |
| **External Integrations** | Yes | ✅ | CodeRabbit gate at PR-open. Linear feature-track issue (filed by user at PR-open time). |

**Constitution gate**: PASS on first evaluation. No violations to track.

## Project Structure

### Documentation (this feature)

```text
specs/004-clang-tidy-cleanup/
├── plan.md              # This file
├── research.md          # Phase 0 output (per-category fix-vs-suppress dispositions)
├── data-model.md        # Phase 1 output (.clang-tidy config schema, suppression/invariant model)
├── quickstart.md        # Phase 1 output (developer-facing how-to-run-tidy)
├── contracts/           # Phase 1 output (per-PR commit-message contract, suppression-rationale contract)
├── checklists/
│   └── requirements.md  # Spec-quality checklist (12/12 PASS, written by /speckit-specify)
└── tasks.md             # Phase 2 output (/speckit-tasks command — NOT created here)
```

### Source Code (repository root) — files added or edited by this feature

```text
nslc/
├── .clang-tidy                                 # MODIFIED — per-category fix/suppress dispositions
│                                               #  + one-line rationale comments
├── .clang-format                               # POSSIBLY MODIFIED — only if a clang-format
│                                               #  violation is judged "config wrong, not source wrong"
├── .specify/memory/constitution.md             # MODIFIED — Principle IX transitional clause removed
│                                               #  (close-out commit, separate from cleanup commits)
├── include/nsl/**/*.h                          # MODIFIED — add [[nodiscard]] (47 sites),
│                                               #  uppercase literal suffix (59 sites),
│                                               #  const-correctness fixes (~150 of 434 sites
│                                               #  in headers; remainder in lib/)
├── lib/**/*.cpp                                # MODIFIED — bulk of the const-correctness +
│                                               #  identifier-naming + include-cleaner work
├── tools/nslc/main.cpp                         # MODIFIED — 5 implicit-bool-conversion fixes
│                                               #  (60-line cap preserved per Principle II)
└── test_unit/**/*.cpp                          # MODIFIED — readability-uppercase-literal-suffix +
                                                #  identifier-naming fixes scoped to test code
```

**Structure Decision**: The cleanup edits ~286 source-checked files across
`include/`, `lib/`, `tools/`, and `test_unit/`. No new files, no
new directories, no removed files. The single project-policy artifact
that actually grows is `.clang-tidy`, which gains explicit per-category
dispositions and rationale comments. The constitution edit
(`.specify/memory/constitution.md`) is the close-out signal and lands
in its own commit per SC-005's "≥ 4 commits, bisectable" requirement.

The plan deliberately does NOT add a baseline-file mechanism (e.g.,
`.clang-tidy-baseline.txt`); per `research.md` §2 below, the chosen
regression-prevention mechanism is config tightening alone (no
per-PR list upkeep, satisfying FR-006's last sentence).

## Complexity Tracking

> No Constitution-Check violations. No complexity entries to record.
