<!-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception -->

# Research: M0 — Build & CI Scaffolding (with P-CI)

**Branch**: `001-m0-build-ci-scaffolding` | **Date**: 2026-04-26
**Plan**: [plan.md](./plan.md)

Each section resolves one Technical Context decision (or one open
plan question) with **Decision / Rationale / Alternatives**. The spec
already pinned the user-visible decisions via `/speckit-clarify`;
this file pins the implementation-mechanism decisions.

---

## §1. Unit-test framework: GoogleTest vs Catch2

**Decision**: **GoogleTest**.

**Rationale**: `docs/design/nsl_compiler_design.md` §13 lists "Catch2
or GoogleTest" — both are constitutional. The tiebreaker is fit with
the LLVM/MLIR/CIRCT ecosystem: LLVM uses GoogleTest natively
(`llvm/utils/unittest/`), CIRCT inherits it, and reusing the bundled
GoogleTest from the LLVM install eliminates a separate dependency.
GoogleTest's `EXPECT_DEATH` and parameterised tests also make the
SPDX-script fixture suite (test_unit/spdx_check_test/) cleaner than
Catch2's equivalents at this size. The header-only convenience of
Catch2 is moot here because we're already linking against the LLVM
toolchain.

**Alternatives**:
- *Catch2*: header-only, simpler standalone integration. Rejected
  because LLVM/CIRCT bundle GoogleTest, so adding Catch2 would create
  a redundant dependency.
- *Google Test + Google Mock*: gMock is overkill at M0 (no
  collaborators to mock yet). Postpone the gMock decision to whenever
  the first collaborator-heavy test lands.

---

## §2. LLVM / MLIR / CIRCT consumption strategy

**Decision**: **Vendored prebuilt** install of LLVM + MLIR + CIRCT,
consumed via `find_package(MLIR REQUIRED CONFIG)` and
`find_package(CIRCT REQUIRED CONFIG)`. CI provisions the prebuilt
via a download+cache step from a project-controlled artifact host
(initially: a tarball checked into a separate `nslc-deps` GitHub
release; later: the CIRCT nightly tarballs once they exist).

**Rationale**: A from-source LLVM build adds 30–60 minutes per CI
invocation per matrix cell — multiplied by 4 cells (`Debug × Release
× {GCC, Clang}`) that is fatal to the SC-007 reviewer-experience
target. A vendored prebuilt lets all 4 cells link in seconds. CIRCT
publishes its CMake `CIRCTConfig.cmake`, so consumption is
LLVM-conventional (`find_package`, `target_link_libraries(... PRIVATE
CIRCTHWDialect ...)`). This keeps the path open for Principle III
(stock CIRCT below the `nsl` dialect) — the build never edits CIRCT
source.

**Alternatives**:
- *Submodule + source build*: traditional but blows the CI budget.
  Rejected.
- *Distro-package install* (apt `llvm-18-dev` etc.): version drift
  vs. CIRCT's pinned commit is a recurring footgun. Rejected.
- *Nix-shell / Bazel*: would work but adds a tooling dependency
  contrary to the spec's "minimal substrate" goal at M0. Postpone.
- *Build LLVM once, cache aggressively*: still needs the first build
  somewhere, and cache evictions break determinism guarantees.
  Rejected.

---

## §3. `add_nsl_library` macro design — borrow from CIRCT

**Decision**: Model `add_nsl_library` on CIRCT's `add_circt_library`
(itself derived from LLVM's `add_llvm_library`). Signature:

```cmake
add_nsl_library(<name>
  <source files...>
  [HEADERS <header files...>]              # one or many; AST exception accommodated
  [DEPENDS <intra-project lib targets...>] # validated against §3 layer table
  [LINK_LIBS <external targets...>]        # MLIR/CIRCT/etc.
  [EXCLUDE_FROM_LIBNSLFRONTEND]            # for tools-only libs (T-track)
  )
```

The macro:
- Calls `add_library(<name> STATIC ...)`.
- Sets `target_compile_features(<name> PUBLIC cxx_std_17)`,
  `set_target_properties(<name> PROPERTIES CXX_EXTENSIONS OFF)`.
- Verifies every entry in `DEPENDS` is a registered nsl-layer that
  is *strictly lower* in the §3 table (FR-004 enforcement). Rejects
  upward and sibling-bypass dependencies at configure time with a
  fatal error citing the §3 row.
- Installs `HEADERS` into the canonical `include/nsl/<Layer>/`
  location.
- Aggregates the library into `libNSLFrontend.a` unless
  `EXCLUDE_FROM_LIBNSLFRONTEND` is set (Principle II reuse).

**Rationale**: Mirroring `add_circt_library` minimizes the cognitive
distance for contributors who already know LLVM/CIRCT idioms, and
inherits years of maturity around install rules, position-independent
code, export-set handling, and aggregate-library patterns.

**Alternatives**:
- *Roll a project-specific minimal macro*: tempting at M0 (we don't
  need everything CIRCT does), but every later milestone would add a
  feature already present in `add_circt_library`. Rejected.
- *Use raw `add_library` everywhere*: violates FR-002 (single
  declaration mechanism) and Principle II's enforcement requirement.
  Rejected.

---

## §4. Determinism toolchain: byte-stable `.a` archives + binary

**Decision**: Apply the LLVM-canonical determinism flag set:

- `CMAKE_AR_FLAGS=Drsc` (or set `CMAKE_C_ARCHIVE_CREATE` /
  `CMAKE_CXX_ARCHIVE_CREATE` to use `ar Drcs`) — `D` is GNU `ar`'s
  deterministic mode (zeroed mtime/uid/gid).
- Linker: `-Wl,--build-id=none` for `nslc` to suppress build-id
  variation; alternatively `--build-id=sha1` (deterministic from
  inputs) — pick `=none` at M0 for simplicity.
- Compiler flags: forbid `__DATE__` / `__TIME__` / `__TIMESTAMP__`
  via a `clang-tidy` rule in `.clang-tidy` (custom check or
  `cppcoreguidelines-avoid-non-const-global-variables`-adjacent).
- `-frandom-seed=$(basename $<)` per object file (catches
  pointer-derived ordering in template instantiation).
- Source-path normalization via `-ffile-prefix-map=<build>=.` (and
  similar for `-fmacro-prefix-map`).
- CMake install rules: explicit `INSTALL` with `FILE_PERMISSIONS`
  (no implicit umask leakage).
- The `git-describe`-derived version string is a configure-time
  constant in the binary (not a build-time `__DATE__`-style macro);
  see §6.

A two-build determinism gate runs as the *last* step of the build
matrix stage: build into `build1/`, build into `build2/`, `diff -r`
the two `lib/` and `bin/nslc` payloads, fail CI on any byte
divergence (FR-018, SC-005).

**Rationale**: This flag set is what LLVM and CIRCT themselves use
to satisfy the analogous reproducibility requirements. The
two-build gate is the cheapest possible enforcement that satisfies
FR-018; running it at M0 catches non-determinism at the moment a
new source file is added rather than discovering it at M5/M7.

**Alternatives**:
- *Trust the toolchain implicitly* (no explicit gate): violates
  Principle V. Rejected.
- *`reprotest`* (Debian's reproducibility harness): heavyweight;
  varies the environment more aggressively than Principle V's
  "input" definition requires. Postpone.

---

## §5. CMake `git-describe` invocation pattern

**Decision**: `cmake/NSLVersion.cmake` runs `execute_process(COMMAND
${GIT_EXECUTABLE} describe --tags --always --dirty
OUTPUT_STRIP_TRAILING_WHITESPACE OUTPUT_VARIABLE NSLC_GIT_DESCRIBE)`
at configure time, with a fallback to `unknown` if `Git_FOUND` is
false (e.g., source-tarball builds). The result is forwarded into
`tools/nslc/Version.cpp.in` via `configure_file(... @ONLY)` so the
binary's version string is fixed at configure time, not at compile
time. A `CMakeLists.txt` `add_custom_target(nsl-reconfigure-on-head)`
re-runs `git describe` if `.git/HEAD` changes, refreshing the
configure cache.

**Rationale**: configure-time injection (rather than compile-time
`__DATE__`-style) keeps `nslc --version` deterministic for a given
ref while still tracking the actual built ref. Tarball fallback is
required because CIRCT, LLVM, and Apache-licensed projects
generally ship source tarballs that don't contain a `.git/`
directory.

**Alternatives**:
- *Compile-time `-DNSLC_VERSION=...` macro*: equivalent in outcome;
  configure-time is preferred because it integrates cleanly with
  install / package recipes.
- *Runtime `popen("git describe")`*: catastrophic — defeats
  determinism, requires git on the deployed host. Rejected.

---

## §6. SPDX-header script: language pick

**Decision**: **Python 3.8+** (`scripts/check_spdx.py`).

**Rationale**: lit is already a Python dependency, so adding a
Python script costs nothing. Python's argparse, pathlib, and clean
exit-code semantics make the script ~50 lines vs ~150 in portable
Bash. Bash struggles with the per-extension comment-syntax dispatch
(FR-010) — a dict lookup in Python is one line. Tests for the script
live in `test_unit/spdx_check_test/` and use GoogleTest's `popen`-
equivalent (`testing::internal::CaptureStdout()` plus `system()`),
*or* — preferred — pure-Python pytest fixtures invoked from CMake
via `add_test(NAME spdx-check-tests COMMAND
${Python3_EXECUTABLE} -m pytest scripts/check_spdx_test.py)`.

**Alternatives**:
- *Pure Bash*: portability across runners is good (every
  GitHub-hosted runner has Bash) but the per-extension dispatch is
  bug-prone. Rejected.
- *C++ tool linked into the build*: way overkill for what's
  fundamentally a regex-on-the-first-line check.
- *Reuse an existing tool* (`reuse`, `licenseheaders`,
  `addlicense`): adds an external dep; none enforce the exact
  expected identifier string with the LLVM-Exception clause out of
  the box. Rejected.

---

## §7. GitHub Actions runner image

**Decision**: **`ubuntu-22.04`** (pinned), not `ubuntu-latest`.

**Rationale**: `ubuntu-latest` is a moving target — image bumps have
broken determinism guarantees in adjacent projects (LLVM has hit
this multiple times). Pinning to `22.04` makes runtime behavior
explicit; we bump in a dedicated PR with clear blast-radius reasoning.
22.04 ships GCC 11 and Clang 14 by default — both well above the
GCC 9 / Clang 10 floor in `nsl_compiler_design.md` §13. We do NOT
install pinned compiler versions at M0; the runner defaults are the
matrix's GCC and Clang values. This is recorded in the build-matrix
contract.

**Alternatives**:
- *`ubuntu-latest`*: rejected (moving target).
- *`ubuntu-24.04`*: newer GCC 13 / Clang 16; safer to wait until
  CIRCT has been validated against it.
- *Self-hosted runners*: out of scope at M0; consider once binary
  caching becomes a runtime bottleneck.

---

## §8. Wired-but-empty stage reporting (FR-015)

**Decision**: Use GitHub Actions' **`if: false`-skipped jobs with an
explicit `wired-but-empty` job name suffix and a status-comment
emitter** that posts (or updates) a single PR sticky-comment listing
"M7 stage: wired but empty (gates by Principle VI 'Reference VCDs'
clause + roadmap M7)" and the equivalent for M8. The job appears in
the required-checks list as "skipped" (which GitHub renders as a
gray dot, distinct from green/red), and the sticky comment ensures
a reviewer who only glances at the PR sees an explicit explanation
without scrolling through logs.

**Rationale**: GitHub's "skipped" status is non-blocking and
visually distinct (FR-015's "not green on no tests, not silently
skipped"). The sticky comment satisfies SC-007 ("reviewer can
identify the failing/empty stage in <10s"). Required-checks status
on a `skipped` job does not block merge but does occupy a row in
the check summary, surfacing the wired state.

**Alternatives**:
- *Always-pass placeholder job*: reads as green, violates FR-015.
- *Always-fail placeholder + bypass each PR*: defeats the merge
  gate. Rejected, hard.
- *Custom GitHub Check Run with `conclusion: neutral`*: cleanest
  semantically but requires a GitHub App; at M0 the sticky-comment
  approach is simpler.
- *Omit stages 5/6 entirely until M7/M8 lands*: violates FR-014's
  "MUST execute … in order" since the stages would not exist.
  Rejected.

---

## §9. Lit topology: one root config + per-directory `lit.local.cfg`

**Decision**: One root `test/lit.cfg.py` defining the project-wide
substitutions (`%nslc`, `%FileCheck`, `%spdx_check`, etc.) and the
test-suffix list (`.test`, `.nsl`); per-layer subdirectories use
`lit.local.cfg` only when they need layer-specific suffixes (e.g.,
`test/Lower/lit.local.cfg` will add `.mlir`-aware substitutions when
M5 lands). At M0 only the root config exists.

**Rationale**: Mirrors LLVM's and CIRCT's lit layout. Adding a layer
test directory in a later milestone requires only dropping files;
no `add_subdirectory` edits, no per-directory configs (FR-007's
"file placement alone").

**Alternatives**:
- *Per-layer root configs*: duplicated boilerplate; rejected.
- *No lit at all at M0; just placeholder test/ dirs*: violates
  FR-007 (each layer must have a passing smoke). Rejected.

---

## §10. Branch-protection configuration: documented + scripted apply

**Decision**: Branch-protection is configured **manually via the
GitHub web UI or `gh api` once**, with the canonical settings
captured in `.github/branch-protection.md`. A helper script
`scripts/apply_branch_protection.sh` invokes
`gh api repos/<owner>/<repo>/branches/main/protection` to apply the
`.github/branch-protection.json` payload (also checked in). The
required-checks list names match the GitHub Actions job IDs from
`ci.yml` exactly. The "Include administrators" / "Do not allow
bypassing the above settings" knobs are ON; only "merge without
waiting for required checks" admin override remains as the
Principle-IX-permitted bypass (spec Q3).

**Rationale**: GitHub's branch-protection API has no first-party
declarative IaC layer that's both stable and lightweight. Checking
in the JSON payload + the apply script + the documentation file
gives us version-controlled config + a one-command re-apply, without
introducing Terraform/Pulumi machinery for a handful of settings.

**Alternatives**:
- *Pure-doc, no script*: no enforcement that the docs match reality.
  Rejected.
- *Terraform / Pulumi / GH IaC tool*: overkill. Postpone to a future
  feature if branch-protection grows beyond a few rules.
- *GitHub Rulesets* (newer than branch-protection): consider once
  GitHub's tooling matures; rulesets currently have rough edges
  (esp. for required-status-check naming). Postpone.

---

## §11. Local-CI entry point: `scripts/ci.sh`

**Decision**: `scripts/ci.sh` is a thin wrapper that runs all six
stages in order, mirroring `ci.yml` step-for-step. Stage selection
via `./scripts/ci.sh <stage-name>` (e.g., `./scripts/ci.sh
static-checks`). Non-zero exit propagates the first failing stage's
exit code. The script reads the same matrix definition (`Debug ×
Release × {GCC, Clang}`) but defaults to the host's compiler (use
`--matrix` to fan out locally). The script is the **authoritative**
definition of what "CI" runs (FR-021); the GitHub Actions YAML is
configured to call into the same shell snippets so divergence
surfaces immediately.

**Rationale**: Bash is the lingua franca of CI; mirroring the
GitHub Actions YAML 1:1 keeps local and remote runs identical. The
`--matrix` flag is for the rare case where a contributor wants to
locally reproduce a cross-compiler failure without GitHub. Defaulting
to single-compiler keeps the common case fast.

**Alternatives**:
- *Makefile target `make ci`*: works but Make's dependency-graph
  semantics confuse CI-stage-ordering reasoning. Rejected for this.
- *`scripts/ci.py`* (Python): no real benefit over Bash here; the
  script is mostly `set -euo pipefail` + `cmake` invocations.
- *No local script; document the manual sequence in
  `CONTRIBUTING.md`*: violates FR-017. Rejected.

---

## §12. Compiler-version pinning within the GCC × Clang matrix

**Decision**: Use the runner image's **default GCC and default
Clang** at M0 (i.e., GCC 11 and Clang 14 on `ubuntu-22.04`). Do
**not** install pinned-floor (GCC 9 / Clang 10) or pinned-ceiling
(latest GCC / latest Clang) versions in the matrix. The build-time
`target_compile_features(<lib> PUBLIC cxx_std_17)` constraint plus
the `.clang-tidy` profile catch the bulk of "you accidentally used
a C++20 feature" mistakes; tighter version pinning is a future
hardening if a real bug slips through.

**Rationale**: The ROI on a 4-build matrix is in the GCC-vs-Clang
*divergence*, not in fine-grained version coverage. Adding floor/
ceiling versions would 2×–4× the matrix for marginal additional
coverage at M0. Q2 explicitly rejected option C (8-build pinned
matrix) for this reason.

**Alternatives**:
- *Floor + ceiling matrix* (option C from spec Q2): rejected by
  user.
- *Newer Ubuntu image with newer compilers*: makes the GCC-9 /
  Clang-10 floor in `nsl_compiler_design.md` §13 untestable. Worse
  than the current default.

---

## §13. Open items deliberately deferred (recorded for plan re-check)

These are **plan-level deferrals**, not unresolved spec ambiguities:

- **CI runtime SLOs.** No numeric SLO at M0 — measure first, set
  numbers later (per spec coverage summary).
- **`bin/` install layout for `nslc`.** Default to
  `${CMAKE_INSTALL_BINDIR}` (i.e., `bin/` under prefix); revisit if
  packagers raise concerns.
- **`gMock` adoption.** Not at M0; revisit when the first
  collaborator-heavy test lands.
- **CIRCT prebuilt artifact host.** Initially `nslc-deps` GitHub
  release; migrate to CIRCT nightlies once available.
- **`docs/` CI gates** (linguist label for `.nsl`, etc.) — these
  are T1 deliverables, not M0 / P-CI. Out of scope here.
- **Pre-commit hook recipe** — that's T12; post-M0.

---

## Phase 0 closing — Constitution re-check

All decisions above have been audited against Constitution v1.4.0
once more after the design was complete. **No principle is
violated.** Notable spot-checks:

- §2 (CIRCT consumption) preserves Principle III's "stock CIRCT
  below the `nsl` dialect" — we consume CIRCT, never edit it.
- §4 (determinism) operationalizes Principle V at the build-system
  layer; the two-build gate enforces SC-005.
- §3 (`add_nsl_library`) operationalizes Principle II; the
  configure-time guard rejects upward / sibling-bypass deps with
  a §3-citing fatal error.
- §6 (Python SPDX script) preserves Principle IX's "no bypass" by
  treating any SPDX failure as a stage-2 failure (no soft-skip).
- §8 (wired-but-empty) preserves FR-015 and Principle IX's stage
  ordering — stages 5 and 6 *exist* and *report*, just with the
  `skipped` conclusion until M7 / M8 land.

**Re-check verdict: PASSES.** Proceed to `/speckit-tasks`.
