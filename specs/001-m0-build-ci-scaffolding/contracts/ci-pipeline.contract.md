<!-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception -->

# Contract: GitHub Actions CI pipeline

**File**: `.github/workflows/ci.yml` (+ branch-protection JSON in `.github/branch-protection.json`)
**Plan**: [../plan.md](../plan.md) §research §7, §8, §10, §11
**Spec FRs covered**: FR-013..FR-022; spec Q2 (matrix), Q3 (bypass), Q4 (SPDX scope), Q5 (version-string format used by stage 1 smoke)

## Triggers

```yaml
on:
  pull_request:
    branches: [main]
  push:
    branches: [main]
```

(spec FR-013)

## Jobs (six required-checks, in Principle IX order)

### Stage 1 — `build-matrix` (4 cells, blocks-merge)

```yaml
strategy:
  fail-fast: false
  matrix:
    build_type: [Debug, Release]
    compiler:   [gcc, clang]
runs-on: ubuntu-22.04
```

(spec Q2; research §7 — `ubuntu-22.04` pinned, not `latest`.)

Steps:
1. Checkout repo at full depth (so `git describe` works).
2. Restore the vendored LLVM/MLIR/CIRCT prebuilt tarball from
   actions/cache (key derived from a checked-in dep-pin file).
3. Configure: `cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=...
   -DCMAKE_C_COMPILER=... -DCMAKE_CXX_COMPILER=... -DMLIR_DIR=...
   -DCIRCT_DIR=...`
4. Build: `cmake --build build`
5. Smoke: `./build/bin/nslc --version` MUST exit 0 and stdout MUST
   match regex `^nslc [0-9A-Za-z._+-]+$` (spec Q5, contract
   `nslc-version.contract.md`).
6. **Determinism gate** (last step, only on the `Release × gcc`
   cell to keep cost bounded): re-build into `build2/`, then
   `diff -r build/lib build2/lib && diff -r build/bin build2/bin`.
   Failure on any byte divergence (FR-018, SC-005, research §4).

Required check name: `build-matrix (Debug, gcc)`,
`build-matrix (Debug, clang)`, `build-matrix (Release, gcc)`,
`build-matrix (Release, clang)` — 4 entries in branch-protection.

### Stage 2 — `static-checks` (1 cell, blocks-merge)

```yaml
runs-on: ubuntu-22.04
needs: build-matrix
```

Steps:
1. Checkout.
2. `clang-format --dry-run --Werror $(git ls-files '*.cpp' '*.h' '*.cc' '*.hpp')` (FR-009).
3. `clang-tidy -p build $(git ls-files '*.cpp')` (FR-008; reuses one matrix cell's compile_commands.json — fetch as artifact from stage 1).
4. `python3 scripts/check_spdx.py $(git ls-files)` — **full repo scan** per spec Q4 (FR-010).
5. Stage exit code = max of the three sub-step exit codes.

### Stage 3 — `unit-and-layer-tests` (1 cell, blocks-merge)

```yaml
runs-on: ubuntu-22.04
needs: build-matrix
```

Steps:
1. Checkout + restore build artifact from stage 1 (`Release × gcc` cell).
2. `ctest --test-dir build --output-on-failure` (runs the GoogleTest
   suites under `test_unit/`).
3. `cd build && lit -v ../test` (runs the per-layer smoke fixtures
   under `test/<Layer>/smoke.test`).

### Stage 4 — `lowering-tests` (1 cell, blocks-merge)

```yaml
runs-on: ubuntu-22.04
needs: build-matrix
```

Steps:
1. Checkout + restore build artifact.
2. `cd build && lit -v ../test/Lower ../test/Driver` — at M0, the only
   lowering-stage fixture is `test/Driver/version.test` (smoke for
   `nslc --version`); from M5 onward this directory grows.

### Stage 5 — `end-to-end` (wired-but-empty, non-blocking at M0)

```yaml
runs-on: ubuntu-22.04
needs: build-matrix
if: false  # body lands at M7; per FR-015 the job is wired but skipped
```

A separate sticky-comment step runs (in a tiny job that does NOT
require `build-matrix`) and posts/updates a PR comment:
"Stage 5 (end-to-end): wired but empty until M7 — see roadmap
M7." (research §8, satisfies SC-007.)

### Stage 6 — `formal` (wired-but-empty, non-blocking at M0)

Same shape as stage 5; sticky-comment cites M8 and Principle VI's
formal clause.

## Required-checks list (branch-protection.json)

```json
{
  "required_status_checks": {
    "strict": true,
    "contexts": [
      "build-matrix (Debug, gcc)",
      "build-matrix (Debug, clang)",
      "build-matrix (Release, gcc)",
      "build-matrix (Release, clang)",
      "static-checks",
      "unit-and-layer-tests",
      "lowering-tests"
    ]
  },
  "enforce_admins": true,
  "required_pull_request_reviews": {
    "dismiss_stale_reviews": true,
    "require_code_owner_reviews": false
  },
  "restrictions": null,
  "allow_force_pushes": false,
  "allow_deletions": false
}
```

`enforce_admins: true` is the spec-Q3 enforcement. Stages 5 and 6
are deliberately NOT in `contexts` while wired-but-empty (otherwise
GitHub would block on a never-firing check). Adding them on M7 / M8
is a one-line PR.

## Cache strategy (FR-018, FR-020)

- LLVM/MLIR/CIRCT prebuilt tarball: cached by content hash of the
  dep-pin file (e.g., `cmake/deps.lock`). Cache hit and cache miss
  produce byte-identical artifacts (spec FR-018, research §4 flag
  set ensures this).
- Build directory: NOT cached at M0. Cache invalidation correctness
  is an unsolved problem at this scale; recompiling the empty
  skeletons takes seconds.
- ccache / sccache: NOT enabled at M0. Revisit if SC-007 reviewer
  experience suffers once real source code lands (M1+).

## Local-CI shape

`scripts/ci.sh <stage>`:

- `./scripts/ci.sh build-matrix [build_type] [compiler]` — single
  cell by default; `--matrix` to fan all 4 cells.
- `./scripts/ci.sh static-checks` — runs the same 3 sub-steps as
  stage 2.
- `./scripts/ci.sh unit-tests` — stage 3.
- `./scripts/ci.sh lowering-tests` — stage 4.
- `./scripts/ci.sh e2e` / `./scripts/ci.sh formal` — exit 0 with
  "wired but empty" message at M0.
- `./scripts/ci.sh all` — runs 1 → 2 → 3 → 4 sequentially; stops at
  first failure with first-failure exit code.

The shell snippets that run inside each stage are **defined in
`scripts/ci.sh`** and the `.yml` calls into them via
`run: ./scripts/ci.sh <stage>` so divergence between local and
remote is impossible (FR-021).
