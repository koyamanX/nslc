<!-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception -->

# Quickstart: T2 — Formatter v0 (`nsl-fmt`)

**Branch**: `010-t2-formatter-v0` | **Date**: 2026-05-04
**Plan**: [plan.md](./plan.md) | **Spec**: [spec.md](./spec.md)

This document onboards a developer to the T2 implementation:
build/test loop, suggested implementation order (Red → Green
→ Refactor per Principle VIII), the documentation updates that
land in the same PR, and the pre-merge checklist.

---

## §1. Prerequisites

- Dev container `ghcr.io/koyamanx/nsl-nslc:dev` is running
  locally (per project memory `project_build_environment.md`).
  No host LLVM installation is required or supported.
- M2 (parser) and M5 (current pipeline) are merged on
  `master`. T2 builds on top of `libNSLFrontend.a` as it
  exists at the T2 PR base.
- This worktree is on branch `010-t2-formatter-v0`; spec and
  clarifications already landed in `4aafcf1`.

```bash
# Inside the dev container:
cd /home/koyaman/devel/nslc
sg docker -c 'docker run --rm -v "$PWD":/work -w /work ghcr.io/koyamanx/nsl-nslc:dev cmake --build build-noasan'
```

(Commands below use the `sg docker -c '…'` wrapper implicitly.)

---

## §2. Build/test loop

```bash
# One-time setup of build-noasan (see project memory libmlir_asan_mismatch):
cmake -G Ninja -B build-noasan -DNSL_ENABLE_ASAN=OFF -DLLVM_DIR=/opt/llvm/lib/cmake/llvm

# Incremental build of just nsl-fmt + its tests:
ninja -C build-noasan nsl-fmt check-nsl-fmt

# Run the lit corpus only:
ninja -C build-noasan check-fmt-lit

# Run the gtest corpus only:
ninja -C build-noasan check-fmt-unit

# Run the determinism CI grep on lib/Fmt/:
./scripts/audit_determinism.sh

# Run the full T2-related CI subset:
./scripts/ci.sh --subset=t2
```

The `check-nsl-fmt`, `check-fmt-lit`, `check-fmt-unit`, and
`--subset=t2` targets are added by T-1 (see §3 below).

---

## §3. Suggested implementation order (TDD — Principle VIII)

The order below is dependency-driven (each step can be merged
once green; later steps assume earlier ones land). Each step
prescribes the *first failing fixture* — Principle VIII
requires the test to be observed failing against the unchanged
tree before the implementation lands.

### T-1: CMake scaffolding + empty `nsl-fmt` binary (Red→Green)

**First failing fixture**: `test/Fmt/cli/version/version-banner.test`
asserts `nsl-fmt --version` prints `nsl-fmt version *`.
Implementation: `add_nsl_executable(nsl-fmt …)` + a 5-line
`main.cpp` that prints `version_string()`. Adds `check-nsl-fmt`
ninja target.

### T-2: `cl::opt` flag plumbing (Red→Green)

**First failing fixture**:
`test/Fmt/cli/mutually-exclusive/check-and-in-place.test`
asserts the §2 frozen string from
[`cli-surface.contract.md`](./contracts/cli-surface.contract.md).
Implementation: declare every `cl::opt` from
`cli-surface.contract.md` §6 + the mutual-exclusion check.

### T-3: TOML config parser + Configuration record (Red→Green)

**First failing fixture**:
`test_unit/Fmt/config_parser_test.cc::DefaultsMatchSpec` asserts
`default_configuration()` returns the §5.1 example values.
Implementation: vendor `toml++` v3.4 under
`third_party/tomlpp/`, write `parse_config_file()`,
`discover_config()`, `default_configuration()`,
`config_key_names()`. Adds `PROVENANCE.md` per Principle V.

### T-4: Directive splitter (Red→Green)

**First failing fixture**:
`test_unit/Fmt/directive_splitter_test.cc::IncludePassthrough`
asserts that `#include "foo.nsl"` produces a `DirectiveTok`
with `opcode = Include` and the byte-for-byte raw text.
Implementation: `lib/Fmt/DirectiveSplitter.{h,cpp}` per
[research §1](./research.md#§1-directive-aware-pre-pass--implementation-shape).

### T-5: CST mode parser extension (Red→Green)

**First failing fixture**:
`test_unit/Fmt/directive_splitter_test.cc::CSTRoundTrip`
asserts `serialize(parse_cst(s)) == s` for a simple `module`
declaration. Implementation: `include/nsl/Parse/CSTMode.h` +
`lib/Parse/CSTMode.cpp` extending `Parser` with `emitCST_`
flag (per [research §2](./research.md#§2-cst-mode-parser-extension--shape-and-reuse)).

### T-6: Doc IR + LayoutRenderer (Red→Green)

**First failing fixture**:
`test_unit/Fmt/doc_layout_test.cc::GroupRibbonFitting`
asserts a small `Group(Concat(Text, Line, Text))` renders
flat at width 100, broken at width 5. Implementation:
`lib/Fmt/Doc.{h,cpp}` + `lib/Fmt/LayoutRenderer.{h,cpp}`.

### T-7: LayoutPlanner — six §5.3 rules (Red→Green ×6)

One sub-step per rule. Each step lands the matching fixture
under `test/Fmt/rules/<rule>/` plus the planner code that
makes it pass. Order:

- T-7a: R5 operator spacing (smallest scope)
- T-7b: R4 bit-slice / concat spacing
- T-7c: R6 attached-comment preservation (load-bearing for
  R1/R2/R3 — must land before alignment rules)
- T-7d: R1 alt/any case-arrow alignment
- T-7e: R2 struct member-bracket alignment
- T-7f: R3 proc_name argument-list wrapping (largest scope)

### T-8: `format_buffer()` top-level entry (Red→Green)

**First failing fixture**:
`test_unit/Fmt/format_parity_test.cc::CLIMatchesLibrary`
asserts the CLI and library produce the same bytes for the
same input + config. Implementation:
`lib/Fmt/Format.{h,cpp}` wires DirectiveSplitter → CSTBuilder
→ LayoutPlanner → LayoutRenderer.

### T-9: `--check` mode + unified-diff emitter (Red→Green)

**First failing fixture**: `test/Fmt/cli/check-mode/dirty-file.test`
asserts a malformatted file produces a unified diff and exit
code 1. Implementation: `lib/Fmt/Diff.{h,cpp}` (Myers
implementation per [research §5](./research.md#§5-unified-diff-emitter--in-tree-vs-library)) + CLI
`--check` branch.

### T-10: `--range LINE:LINE` (Red→Green)

**First failing fixture**: `test/Fmt/cli/range/inside-alt-block.test`
asserts that `nsl-fmt --range 5:7 file.nsl` reformats only
lines 5–7. Implementation: extend `format_buffer()` to take
`std::optional<LineRange>` and constrain layout to the
selected sub-CST.

### T-11: Multi-file continue-on-error (Red→Green)

**First failing fixture**:
`test/Fmt/cli/multi-file/one-bad-one-good.test` asserts that
when one of two files has a parse error, the good file still
gets formatted, the bad file's diagnostic appears on stderr,
and exit code is 1. Implementation: per-file try/catch in
`tools/nsl-fmt/main.cpp` + aggregate failure tracking.

### T-12: Edge cases + idempotence sweep (Red→Green ×N)

Land each edge-case fixture (empty input, BOM, CRLF, parse-
error refusal, over-long line) plus the synthetic
idempotence corpus. Land the audited-corpus fixture wiring
with `UNSUPPORTED:` markers per [research §6](./research.md#§6-audited-corpus-availability--current-vs-t2).

### T-13: CI integration (Red→Green)

Add `nsl-fmt --check test/audited/**/*.nsl` to `scripts/ci.sh`
guarded by `|| true` (becomes a hard gate at M7). Add the
`audit_fmt_api.sh` script enforcing the 10-symbol surface from
[`format-api.contract.md`](./contracts/format-api.contract.md)
§5. Extend `scripts/audit_determinism.sh` to scan
`lib/Fmt/`.

### T-14: Documentation updates (no test — see §9)

Land the three doc updates in §9 below in the same PR.

---

## §4. Suggested git workflow

T-1..T-14 are 14 commits on `010-t2-formatter-v0`. Each commit:

- Adds the failing test FIRST (one commit).
- Implements the feature (one commit).
- Lands the doc / CI changes (one commit, batched at the end
  of each T-step where applicable).

Per Principle VIII "no retrofitted tests": the test commit
MUST precede the implementation commit. A squash-merge to
`master` MUST record the failing-state commit hash in the PR
description.

---

## §5. Determinism verification recipe

Before opening the T2 PR, run:

```bash
# Build twice in different paths; verify byte-identical artifacts.
mkdir -p /tmp/build-{a,b}
for dir in /tmp/build-a /tmp/build-b; do
    cmake -G Ninja -B "$dir" -DNSL_ENABLE_ASAN=OFF -DLLVM_DIR=/opt/llvm/lib/cmake/llvm /home/koyaman/devel/nslc
    ninja -C "$dir" nsl-fmt
done
diff -q /tmp/build-a/bin/nsl-fmt /tmp/build-b/bin/nsl-fmt

# Format every audited file twice; verify byte-identical output (idempotence).
for f in test/audited/**/*.nsl; do
    out1=$(./build-noasan/bin/nsl-fmt --stdin < "$f")
    out2=$(echo "$out1" | ./build-noasan/bin/nsl-fmt --stdin)
    [ "$out1" = "$out2" ] || { echo "IDEMPOTENCE FAIL: $f"; exit 1; }
done
```

The build-twice-then-diff is the project's standard
determinism gate (see `scripts/audit_determinism.sh`).

---

## §6. Phase 2 polish (deferred — not blocking T2 acceptance)

Items deliberately out-of-scope for T2 acceptance, listed
here so they don't get forgotten:

- `Doc` factory methods returning `std::shared_ptr<const Doc>`
  for full const-correctness (data-model §4 placeholder).
- `--write-mode=atomic-or-fsync` for stronger atomicity (not
  needed at T2; current rename-after-temp is sufficient).
- Sublime/Emacs invocation snippets for `nsl-fmt -i` (T11
  packaging milestone).
- Hooked into pre-commit (T12 milestone — out of scope per
  CLAUDE.md §2.3).
- LSP `textDocument/formatting` (T5 — by design out of scope).

---

## §7. Pre-merge checklist (for the T2 PR)

- [ ] All 14 T-steps' failing fixtures landed BEFORE their
  implementation commits (Principle VIII).
- [ ] `ninja -C build-noasan check` is green inside the dev
  container.
- [ ] `./scripts/audit_determinism.sh` reports zero
  hits.
- [ ] `./scripts/audit_fmt_api.sh` reports exactly 10 public
  symbols.
- [ ] Two-build determinism diff (§5) returns empty.
- [ ] Audited-corpus idempotence sweep (§5) succeeds on every
  vendored project (currently UNSUPPORTED until M7; add
  `|| true` for now).
- [ ] CodeRabbit review has run (Principle IX) and all
  blocking findings are addressed.
- [ ] PR description references the originating Linear issue
  (`Closes NSLC-<N>` once one exists; T2 may have been opened
  ad-hoc — file a Linear issue if missing per the
  application-criteria table in `CONTRIBUTING.md` §3.3).
- [ ] PR description includes the failing-state commit hash
  for each TDD step (Principle VIII squash-merge clause).
- [ ] AI-assisted commits carry `Assisted-by: Claude-Code:claude-opus-4-7`
  per `CONTRIBUTING.md` §5.2.
- [ ] No `Signed-off-by` on AI-authored commits (the human
  submitter adds theirs at PR-merge time).

---

## §8. Failure modes — what to do when

| Symptom | Likely cause | Fix |
|---|---|---|
| `check-nsl-fmt` fails at first `audit_fmt_api.sh` | Added a public symbol to `Fmt.h` without amending `format-api.contract.md` §5 | Either revert the symbol or amend the contract in the same change |
| `audit_determinism.sh` flags `lib/Fmt/Foo.cpp` | Used `std::unordered_*` or pointer-derived ordering | Switch to `std::map` (sorted) or another deterministic container |
| Idempotence sweep fails on a synthetic fixture | The CST round-trip (cst-shape contract §8) is broken — leaf trivia is being dropped or duplicated | Add a unit test for the specific edge case; trace the `serialize()` invariant |
| `format_parity_test.cc::CLIMatchesLibrary` fails | The CLI and library are computing different config / range / file IDs | The CLI's `format_buffer()` invocation must be byte-identical to the library's; see the test's golden values |
| Lit fixture for an alignment rule has post-format bytes that don't match | The planner is emitting different padding than the contract specifies | Re-read `formatting-rules.contract.md` §1 / §2 / §3 — the column-calculation rule is frozen there |
| Audited-corpus fixtures all marked UNSUPPORTED | Expected at T2 (research §6); they activate when M7 lands | No action needed at T2 |

---

## §9. Documentation updates landing in the T2 PR

Per Principle VII, T2 amends three docs in the same PR as the
implementation:

1. **`CLAUDE.md` (project root)** — the `<!-- SPECKIT START -->`
   block updates from `008-m5-structural-passes` to
   `010-t2-formatter-v0` (handled automatically by
   `/speckit-plan`'s agent-context-update step). The §2.3
   tooling-feature roll-up footnote gets a one-line addition
   noting the directive-aware pre-pass scope decision.

2. **`docs/design/nsl_tooling_design.md` §5.2** — the
   architecture diagram is amended to show the directive-
   aware pre-pass as a stage *before* the CST-mode parser:

   ```
   Source text
        │
        ▼
   ┌────────────────────┐
   │ DirectiveSplitter  │  ← NEW (research §1)
   │  (raw → directive  │
   │   | NSL fragment)+ │
   └─────┬──────────────┘
         │ Directives ➝ opaque  ─┐
         │ NSL fragments         │
         ▼                       │
   ┌──────────┐                  │
   │  Lexer   │ (existing)       │
   └─────┬────┘                  │
         │                       │
         ▼                       │
   ┌──────────┐                  │
   │  Parser  │ + emitCST_=true  │
   │ (CST mode│                  │
   │  via NEW │                  │
   │ CSTMode) │                  │
   └─────┬────┘                  │
         │ CST                   │
         ▼                       │
   ┌──────────────────────┐      │
   │ LayoutPlanner /      │ <────┘
   │ LayoutRenderer       │
   └─────┬────────────────┘
         │
         ▼
   Formatted text
   ```

3. **`docs/CLAUDE.md` §7** — line ranges for the §5 section of
   `nsl_tooling_design.md` are recomputed if the §5.2 amendment
   shifts boundaries (likely a +20-line shift — re-grep
   `^### \|^## ` after editing).

---

## §10. CI gate map

Per Principle IX § Pipeline stages, T2 deliverables exercise:

| Stage | What runs at T2 | New at T2 |
|---|---|---|
| 1. Build matrix | `ninja nsl-fmt check-nsl-fmt` on Debug + Release | `nsl-fmt` binary, `libNslFmt.a`, `tomlpp` vendor |
| 2. Static checks | `clang-format`, `clang-tidy`, SPDX header check + `audit_determinism.sh` (now scans `lib/Fmt/`) + `audit_fmt_api.sh` (10-symbol gate) | Two new audit scripts |
| 3. Unit & layer | gtest `test_unit/Fmt/` (5 cases) | `format_parity_test`, `directive_splitter_test`, `config_parser_test`, `doc_layout_test`, `diff_emitter_test` |
| 4. Lowering tests | lit `test/Fmt/` (~30 fixtures) | All `test/Fmt/` fixtures |
| 5. End-to-end | (vacuous for T2 — formatter does not produce Verilog) | nothing |
| 6. Formal | (vacuous for T2 — formatter does not engage formal tools) | nothing |

The `nsl-fmt --check test/audited/**/*.nsl` step in
`scripts/ci.sh` is added by T-13, guarded by `|| true` until
M7 vendors the audited projects. After M7, the `|| true`
guard is removed in a one-line follow-up commit and the gate
becomes hard.

---

**Output gate**: ✅ Quickstart complete; ready for
`/speckit-tasks` to expand T-1..T-14 into per-task work
items.
