<!-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception -->

# Quickstart: Implementing M6 (`nsl-lower` part 2 — `nsl` → CIRCT)

**Branch**: `010-m6-circt-lowering` | **Date**: 2026-05-04
**Spec**: [spec.md](./spec.md) | **Plan**: [plan.md](./plan.md)

This is the developer-onboarding pointer for contributors picking
up M6 work. Read this first; it tells you where every artefact
lives, how to run the build/test loop, and what the first
TDD-failing-fixture looks like.

---

## 1. Read order

In order, top-down:

1. [`spec.md`](./spec.md) — what is being built (US1–US5,
   FR-001 … FR-034, SC-001 … SC-008, Clarifications Q1–Q3 +
   specify-time Q1–Q2)
2. [`plan.md`](./plan.md) — Technical Context, Constitution
   Check gate matrix, Project Structure, source-tree decisions
3. [`research.md`](./research.md) — every "deferred to plan"
   ambiguity resolved with Decision/Rationale/Alternatives
   (15 sections)
4. [`data-model.md`](./data-model.md) — class shapes, op
   relationships, fixture taxonomy, M5/M4 freeze surfaces
5. [`contracts/`](./contracts/) — frozen public-API surfaces
   (touch these, you are amending the M6 contract):
   - [`lower-api.contract.md`](./contracts/lower-api.contract.md)
     (10-symbol public surface; M5 +2)
   - [`circt-lowering.contract.md`](./contracts/circt-lowering.contract.md)
     (per-op mapping table — design §10 freeze)
   - [`driver-emit-hw.contract.md`](./contracts/driver-emit-hw.contract.md)
     (CLI flag freeze; `-emit=hw` and `-emit=circt` alias)
   - [`firreg-convention.contract.md`](./contracts/firreg-convention.contract.md)
     (Q2 reset polarity + Q3 mux-on-data conventions)

If a question survives reading these documents, run
`/speckit-clarify` again — do not improvise.

**Background reading** (M-track context):

- [`docs/design/nsl_compiler_design.md`](../../docs/design/nsl_compiler_design.md)
  §10 lines 1202–1267 (the canonical mapping table that
  `circt-lowering.contract.md` freezes by reference)
- [`README.md`](../../README.md) §Roadmap M6 row (the milestone
  literal text — test gate + constitutional anchors)
- M5 plan + research + data-model under
  [`specs/008-m5-structural-passes/`](../008-m5-structural-passes/)
  (M6 inherits much of the surrounding scaffolding from M5;
  knowing how M5 built it helps)

---

## 2. Environment setup

The dev container is canonical (per memory:
`ghcr.io/koyamanx/nsl-nslc:dev`, locally cached):

```bash
# From repo root, all build/test commands run inside the container:
sg docker -c "docker run --rm -v \$PWD:/work -w /work \
  ghcr.io/koyamanx/nsl-nslc:dev \
  bash -c 'cmake -B build-noasan -G Ninja -DCMAKE_BUILD_TYPE=Debug \
                 -DNSL_ENABLE_ASAN=OFF \
                 && cmake --build build-noasan'"
```

Use `build-noasan` per memory note — the vendored libMLIR was
built without ASan, so instrumented binaries fire use-after-poison
on MLIRContext construction. The lit cell in CI also sets
`NSL_ENABLE_ASAN=OFF`.

Verify the M5 baseline works first:

```bash
sg docker -c "docker run --rm -v \$PWD:/work -w /work \
  ghcr.io/koyamanx/nsl-nslc:dev \
  bash -c 'cd build-noasan && lit -v test/Lower/'"
# Expect: ~499 PASS + ~7 XFAIL (the M5 acceptance state).
```

---

## 3. The first TDD-failing fixture

Follow Principle VIII: test first, observed failing, then
implement.

The first fixture to author: `test/Lower/circt/arith/add.nsl`
(simplest leaf-op pattern):

```nsl
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// RUN: %nslc -emit=hw %s | %FileCheck %s

declare M {
  input a[8];
  input b[8];
  output q[8];
}

module M {
  q = a + b;
}

// CHECK-LABEL: hw.module @M
// CHECK-DAG:   %{{.+}} = comb.add %a, %b : i8
// CHECK-NOT:   nsl.add
```

Running `lit -v test/Lower/circt/arith/add.nsl` should fail with
"command not found: -emit=hw" (the flag does not yet exist).

**Implementation order** (TDD-driven):

1. Wire the `-emit=hw` flag in `tools/nslc/main.cpp` and
   `lib/Driver/Compilation.cpp`. Implement
   `Compilation::lowerToCIRCT` returning `mlir::failure()`
   unconditionally as a stub. The fixture now fails with
   "conversion failed" — observed-failing in a different mode.
2. Implement the bare `NSLToCIRCTPass` shell (no patterns
   registered). Add a smoke test asserting `nsl-opt
   -nsl-to-circt empty.mlir` parses and runs. The arith fixture
   still fails (no `nsl.add` pattern).
3. Implement `populateArithPatterns` with one
   `OpConversionPattern<nsl::AddOp>` mapping to `comb::AddOp`.
   Re-run the fixture — should pass.
4. Move to the next fixture (`sub.nsl`, `mul.nsl`, …) following
   the design-§10 mapping table.

This sequence walks the fixture corpus row by row in dependency
order: leaf arith first, then bit-ops, then control flow, then
state elements, then FSM, then sim, then module-level, then
round-trip.

---

## 4. The CI coverage guard (FR-033)

Before declaring M6 complete, the CI coverage guard MUST report
zero gaps. Add `coverage_guard.cmake` early so it gates partial
work:

```cmake
# test/Lower/circt/coverage_guard.cmake (skeleton)
file(GLOB pattern_files
     ${CMAKE_SOURCE_DIR}/lib/Lower/CIRCTPatterns/*.cpp)
file(GLOB fixture_files
     ${CMAKE_SOURCE_DIR}/test/Lower/circt/*/*.nsl)

# Walk pattern_files for `add<NSLToCIRCT_*_Pattern>(...)` lines;
# walk fixture_files for filenames; assert bijection.

# On mismatch: message(FATAL_ERROR "...")
```

The guard fires at configure time, so `cmake -B build-noasan`
itself fails on a coverage gap. This is intentional — a
contributor cannot land a pattern without a fixture or vice
versa.

---

## 5. Iteration loop per pattern

For each design-§10 row:

```bash
# 1. Author the fixture (TDD: failing first):
$EDITOR test/Lower/circt/<family>/<op>.nsl
$EDITOR test/Lower/circt/<family>/<op>.mlir.expected

# 2. Run lit, observe failure:
sg docker -c "docker run --rm -v \$PWD:/work -w /work \
  ghcr.io/koyamanx/nsl-nslc:dev \
  bash -c 'cd build-noasan && lit -v test/Lower/circt/<family>/<op>.test'"

# 3. Author the pattern in the corresponding family file:
$EDITOR lib/Lower/CIRCTPatterns/<Family>Patterns.cpp

# 4. Re-build + re-lit:
sg docker -c "docker run --rm -v \$PWD:/work -w /work \
  ghcr.io/koyamanx/nsl-nslc:dev \
  bash -c 'cd build-noasan && cmake --build . && \
           lit -v test/Lower/circt/<family>/<op>.test'"

# 5. Move to next row.
```

Throughout the loop, the determinism gate runs in CI: any
non-byte-stable output across two builds fails immediately.

---

## 6. Common pitfalls

- **Forgetting `mlir::Location` propagation**: `OpConversionPattern`
  patterns must explicitly source location from the source op
  (`op->getLoc()`); the rewriter does NOT auto-propagate.
  Catching this at fixture authoring time saves a fixture-level
  CI grep failure later (FR-030).
- **`!nsl.struct<@T>` reaching M6**: the M5 `NSLExpandVariablesPass`
  must be running. If you see `!nsl.struct` in M6 input, your M5
  pipeline has regressed. Check `nslc -emit=mlir` first.
- **Missing `getDependentDialects` registration**: the pass needs
  to declare every CIRCT dialect it produces ops in, else the
  PassManager won't load them. Forgetting `circt::sv::SVDialect`
  is the most common omission (only sim fixtures hit it).
- **`-emit=hw` invoking stock CIRCT passes**: per Q2-specify-time
  → A, M6 halts strictly at the conversion boundary. Do NOT add
  `--canonicalize` or any `circt::*` pass to the M6 pipeline.
- **`hwarith` adoption**: per Q1 → A, M6 is `comb`-only. Reject
  any reviewer suggestion to "use `hwarith.add` for clarity";
  fixtures and goldens are pinned to `comb`.
- **Reset polarity drift**: per Q2 → C, async-active-low is the
  no-`interface` default. Fixtures asserting `posedge rst` will
  break the audited-corpus alignment.

---

## 7. Done criteria (M6 acceptance)

M6 is complete when:

1. [ ] All ~75–85 fixtures under `test/Lower/circt/` pass under
   lit + FileCheck.
2. [ ] `coverage_guard.cmake` reports zero pattern↔fixture gaps.
3. [ ] CI determinism gate passes on `-emit=hw` outputs (two
   host paths produce byte-identical output).
4. [ ] US5 round-trip fixtures pass `nslc -emit=hw … |
   circt-opt --convert-fsm-to-seq --lower-seq-to-sv
   --prepare-for-emission` with zero diagnostics.
5. [ ] `Lower.h` has exactly 10 symbols (M5's 8 + M6's 2).
6. [ ] `lib/Lower/CMakeLists.txt` adds `CIRCTFSM` to
   `LINK_LIBS` (and only `CIRCTFSM`).
7. [ ] CodeRabbit blocking findings addressed.
8. [ ] PR description references the M6 spec + plan + the
   originating Linear issue.

After M6 lands, the SPECKIT block in root `CLAUDE.md` flips to
`011-m7-driver-end-to-end` (or whatever M7's branch number
becomes).
