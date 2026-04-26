<!-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception -->

# Quickstart: M0 — Build & CI Scaffolding (with P-CI)

**Branch**: `001-m0-build-ci-scaffolding` | **Date**: 2026-04-26
**Plan**: [plan.md](./plan.md) | **Spec**: [spec.md](./spec.md)

The minimum sequence a contributor follows to verify the M0
deliverables on a fresh clone. This file is the source of truth
behind the M0 acceptance scenarios in `spec.md` US1 / US2 / US3.

---

## Prerequisites

On a Linux x86_64 host:

- **CMake** ≥ 3.22
- **Ninja** (optional but recommended; the plan defaults to `-G Ninja`)
- **GCC** ≥ 9 *or* **Clang** ≥ 10 (the GitHub Actions matrix tests both; locally either works)
- **Python** ≥ 3.8 (for `lit` and `scripts/check_spdx.py`)
- **Git** (any reasonably recent version; `git describe --tags --always --dirty` is the only invocation)
- **LLVM + MLIR + CIRCT prebuilt tarball** — initial setup downloads it via the project's `nslc-deps` GitHub release (URL pinned in `cmake/deps.lock`).

`gh` (GitHub CLI) is **not** required to build, but is required if
you want to apply branch-protection from a checkout (see §Apply
branch protection below).

---

## 1. Clone

```bash
git clone https://github.com/<owner>/nslc.git
cd nslc
```

---

## 2. Configure & build

```bash
cmake -S . -B build -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DMLIR_DIR=/path/to/llvm-install/lib/cmake/mlir \
  -DCIRCT_DIR=/path/to/circt-install/lib/cmake/circt
cmake --build build
```

Expected outcome (US1 acceptance scenario 1): all nine static
library archives appear under `build/lib/nsl/<Layer>/`, and
`build/bin/nslc` is produced. Configure log includes
`-- Found Git: ...` and `-- nslc version: nslc <git-describe>`
emitted by `cmake/NSLVersion.cmake`.

---

## 3. Smoke: `nslc --version`

```bash
./build/bin/nslc --version
```

Expected (US1 acceptance scenario 2; spec Q5; SC-002 < 100 ms):

```
nslc 0.0.0-dev+g<short-sha>
```

(or `nslc 0.0.0-dev+g<short-sha>-dirty` if your working tree has
uncommitted changes). Exit code `0`.

---

## 4. Run the tests

```bash
ctest --test-dir build --output-on-failure          # GoogleTest unit fixtures (test_unit/)
cd build && lit -v ../test                           # per-layer lit smokes + lowering smokes
```

Expected: 9 layer smoke fixtures + the driver smoke
(`test/Driver/version.test`) all pass; the GoogleTest suite for
`scripts/check_spdx.py` and the `add_nsl_library` macro all pass.

---

## 5. SPDX header check (FR-010, spec Q4)

```bash
python3 scripts/check_spdx.py --all
```

Expected on a clean tree: `spdx-check: <N> passed, 0 failed, 0
exempt (out of <N> files)`, exit 0.

To validate FR-010's fail-loud behavior:

```bash
echo "int main() {}" > /tmp/foo.cpp
python3 scripts/check_spdx.py /tmp/foo.cpp
```

Expected: exit 1, diagnostic names `/tmp/foo.cpp:1` and prints both
expected and observed lines.

---

## 6. Local CI reproduction (FR-017, FR-021)

```bash
./scripts/ci.sh all                                  # runs stages 1→4 sequentially
./scripts/ci.sh build-matrix Debug clang             # single matrix cell
./scripts/ci.sh static-checks                        # just stage 2
./scripts/ci.sh build-matrix --matrix                # fan out all 4 cells locally
```

Expected on a clean tree at the same git ref where remote CI
reports green: every invoked stage exits 0 (US2 acceptance scenario
2; SC-006).

`./scripts/ci.sh e2e` and `./scripts/ci.sh formal` print the
"wired but empty" status and exit 0 — no audited-project bodies
land until M7 / M8 (FR-015, plan §research §8).

---

## 7. Apply branch protection (one-time, requires repo admin)

```bash
gh auth login                                        # if not already
./scripts/apply_branch_protection.sh                 # POST .github/branch-protection.json to the API
```

Expected: branch protection on `main` is now active with
required-checks for the seven contexts named in
`contracts/ci-pipeline.contract.md`, `enforce_admins: true`, and
`allow_force_pushes: false` / `allow_deletions: false`. The only
permitted bypass is GitHub's repo-admin "merge without waiting for
required checks" override, which by spec Q3 / FR-016 MUST be
accompanied by a named reason in the PR description.

This step is **one-time setup**; subsequent runs are idempotent
(the `gh api` PATCH/POST will surface "no changes" if the config
already matches).

---

## 8. Two-build determinism gate (FR-018, SC-005, plan §research §4)

```bash
cmake -S . -B build1 -G Ninja -DCMAKE_BUILD_TYPE=Release && cmake --build build1
cmake -S . -B build2 -G Ninja -DCMAKE_BUILD_TYPE=Release && cmake --build build2
diff -r build1/lib build2/lib && diff -r build1/bin build2/bin
```

Expected: `diff` exits 0 with no output. Any byte divergence is a
non-determinism leak per Principle V; the CI build-matrix stage's
last step performs this comparison automatically (on the
`Release × gcc` cell to keep cost bounded).

---

## What is NOT yet possible (intentionally — these land later)

| Capability | Lands at |
|---|---|
| `nslc input.nsl -emit=tokens` (lexer driver) | M1 |
| `nslc input.nsl -emit=ast` (parser driver) | M2 |
| Sema diagnostics (`S1`–`S29`) | M3 |
| `nsl-opt` round-trip | M4 |
| `nslc input.nsl -emit=mlir` / `-emit=hw` | M5 / M6 |
| End-to-end audited-project compile + simulate | M7 (gates also need `P-VEN` + `P-VCD`) |
| `riscv-formal` integration | M8 |
| `nsl-fmt` / `nsl-lsp` / `nsl-lint` | T2 / T3 / T6 |
| TextMate / tree-sitter grammars | T1 / T8 |

At M0, `nslc` knows exactly one trick: print its version. That is
deliberate. The substrate this milestone lays — the layered build,
the deterministic toolchain, the six-stage CI pipeline, the
license-and-style gates — is what every later milestone consumes
without ever revisiting.
