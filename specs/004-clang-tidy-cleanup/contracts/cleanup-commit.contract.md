<!-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception -->
# Contract: Per-Category Cleanup Commit

**Scope**: contract that every per-category cleanup commit on
`004-clang-tidy-cleanup` MUST satisfy. This is the bisect-friendly
audit trail SC-005 requires.

## Pre-conditions (asserted before commit)

1. The tree at `HEAD` builds clean inside
   `ghcr.io/koyamanx/nsl-nslc:dev` via `cmake --build build-Release-host`.
2. The `static-checks` stage of `./scripts/ci.sh` reports a higher-or-
   equal warning count at `HEAD` than what the commit will produce.
   (Equal is allowed only for the constitution close-out commit, whose
   purpose is doc-only.)
3. The full lit suite (118 fixtures) passes at `HEAD`.
4. The full ctest suite (129 cases) passes at `HEAD`.
5. The SPDX header check (`scripts/check_spdx.py --all`) reports
   `286 passed, 0 failed, 127 exempt` (the M3 baseline).

## Commit shape (FR-011)

The commit MUST satisfy the schema in `data-model.md` Entity 3:

- Title line: `<type>(<scope>): T<NNN> — <category> cleanup (<count> sites, <files> files)`
- Body: one-paragraph context + categories-cleared block + verification
  block + `Co-Authored-By` trailer.

**Title-line constraints**:

- `<type> ∈ {chore, style, refactor}`
- `<scope>` matches the bulk-edit directory (e.g., `include`,
  `lib/preprocess`)
- `<category>` is the literal clang-tidy category name
- `<count>` is the warning-site delta (pre - post)
- `<files>` is `git diff --stat HEAD~1 HEAD | tail -1`'s file count

**Body constraints**:

- `Categories cleared in this commit:` block has one line per
  category fixed, with format `- <category>: <count> warnings (was <previous>)`
- `Verification inside ghcr.io/koyamanx/nsl-nslc:dev:` block has
  three lines: static-checks delta, lit pass count, ctest pass count.
- `Co-Authored-By:` trailer is mandatory per the project's AI-attribution
  policy.

## Post-conditions (asserted after commit)

1. The tree at `HEAD` builds clean inside the canonical container.
2. The `static-checks` total warning count at `HEAD` is STRICTLY
   LESS than at `HEAD~1` (with the constitution-close-out commit
   exception above).
3. The full lit suite still passes (118/118).
4. The full ctest suite still passes (129/129).
5. SPDX header check still passes (`286/0/127`).
6. No new TODO/FIXME/HACK/XXX markers introduced (FR-009).
7. No public-API symbol added or removed in `include/nsl/**/*.h`
   (FR-010). Adding `[[nodiscard]]` is allowed; renaming or
   re-typing is not.

## Invariants across the commit sequence

- The first commit is the clang-format sweep (per `research.md` §3).
- The last commit is the constitution close-out edit (per `research.md`
  §3 step 10 + SC-004).
- Every intermediate commit reduces the static-checks warning count.
- A `git bisect` across the feature branch lands on a buildable, lit-
  green, ctest-green tree at every intermediate commit (SC-005).
- No commit re-enables a globally-suppressed category in `.clang-tidy`
  via `// NOLINTNEXTLINE` (data-model.md §5 cross-entity invariant).

## Verification

The commit author asserts each pre-condition by running the gate before
committing; CI re-asserts each post-condition on push. CodeRabbit and
Copilot review at PR-open time are the third defensive layer.
