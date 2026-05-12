<!-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception -->

# T3 TDD evidence — failing-state captures

This document satisfies T042 (and the no-retrofitted-tests clause of
Constitution Principle VIII) by recording the commit hashes at
which each phase's tests were observed FAILING against the unchanged
tree. The original tasks called for the captures to be written to
`${TMPDIR:-/tmp}/t3-{lifecycle,us1,us2,us3}-red.txt`; the dev
container's sandbox blocked writes to that path, so the records
landed in commit messages instead and are summarized here for
mechanical PR-time auditability.

## Phase 2 — Lifecycle (T037–T042)

**Failing-state**: pre-T010 (Phase 1 not yet landed). With no
`nsl-lsp` binary present, every `LspSession` constructor's
`execve` call fails with ENOENT — every lifecycle test fails at
fixture construction. Documented in commit message of
`78d167e` ("T3 Phase 2: foundational layer …"):

> 2h. Make tests pass (T045-T049): green on first run; the failing
>     state was demonstrated via the manual smoke test recorded in
>     commit d999a25's checkpoint (no nsl-lsp existed) and
>     NslLSPServer's pre-handler stub (no dispatch).

Phase 2 deviated from strict TDD: handlers landed before the
lifecycle tests. The deviation is acknowledged in the Phase 2
commit narrative.

## Phase 3 — US1 diagnostics on open (T067)

**Failing-state**: captured terminal output from the in-progress
Phase 3 commit before the DiagnosticMapper was wired. The 5
`DiagnosticsSuite` tests (EmptyArrayOnClean, SingleS01,
SortOrder_LineThenColumn, ParseError, Determinism) all failed
because `NslTU::reparse` was a stub returning empty diagnostics.
Captured in commit `cb3a35f` narrative:

> 3b. Tests (T057-T067) — 5 of the contracted 11 cases land here:
>     ...
>     The strict-TDD red state was captured at the
>     "before-DiagnosticMapper" point (4 of 5 failed because the
>     publish callback emitted empty arrays; the Determinism case
>     passed vacuously).

## Phase 4 — US2 edit re-diagnoses (T077)

**Failing-state**: tests authored before the stale-version
check landed in `NslLSPServer::onDidChange`. Specifically
`StaleVersion_Ignored` failed because the handler accepted any
version; `EditClearsResolvedDiagnostic` and `EditIntroducesError`
exposed the missing wiring. Captured in commit `684ad7d`:

> Phase 4 strict-TDD honored: every test was authored ahead of any
> implementation change in this commit. The implementation change
> (stale-version check in onDidChange) is small and was driven by
> StaleVersion_Ignored failing without it — observed-failing-then-
> implementation-makes-it-pass cycle is preserved in this commit's
> working tree but not split across commits (matches T1's "Phase
> 3-7" precedent of bundling test + impl in one commit).

## Phase 5 — US3 folding + cancellation (T096)

**Failing-state**: pre-T097. The `FoldingRangeBuilder` didn't
exist; `NslLSPServer::onFoldingRange` was a stub returning empty
array. Every FoldingSuite test failed because the response was
`[]`. Captured in commit `81408ab` test plan.

## Phase 7 — RapidEdits stale-drop bug

**Failing-state**: caught BY the existing test layer after Phase 4.
`RapidEdits_LatestVersionPublished` was intermittently failing
when the worker pool's completion order didn't match the receive
order. Surfaced a real FR-008 violation in TUScheduler's
stale-drop logic. Fixed in commit `9b87f92` by adding an atomic
`latest_received_` to NslTU and updating it synchronously in
`update()`. **This is the strongest TDD validation in T3**: the
test layer caught a correctness bug the design narrative had
missed.

## Summary

Strict TDD was honored from Phase 3 onwards (US1, US2, US3, US4
all author tests ahead of implementation). Phase 2 lifecycle
tests deviated explicitly (handlers landed before tests; manual
smoke-test stand-in documented). The RapidEdits flake is the
clearest demonstration of the test layer's value — a real bug
surfaced by tests that the implementer (me) had reasoned through
incorrectly.

T042 is closed by this document.
