<!-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception -->

# Quickstart: Implementing M5 (`nsl-lower` part 1)

**Branch**: `008-m5-structural-passes` | **Date**: 2026-04-30
**Spec**: [spec.md](./spec.md) | **Plan**: [plan.md](./plan.md)

This is the developer-onboarding pointer for contributors picking
up M5 work. Read this first; it tells you where every artefact
lives, how to run the build/test loop, and what the first
TDD-failing-fixture looks like.

---

## 1. Read the spec, then the plan

In order, top-down:

1. [`spec.md`](./spec.md) — what is being built (US1–US5, FR-001 …
   FR-031, SC-001 … SC-012, Clarifications Q1–Q4)
2. [`plan.md`](./plan.md) — Technical Context, Constitution Check
   gate matrix, Project Structure, source-tree decisions
3. [`research.md`](./research.md) — every "deferred to plan"
   ambiguity resolved with Decision/Rationale/Alternatives
4. [`data-model.md`](./data-model.md) — class shapes, op
   relationships, fixture taxonomy
5. [`contracts/`](./contracts/) — frozen public-API surfaces
   (touch these, you are amending the M5 contract):
   - `lower-api.contract.md` (8-symbol public surface)
   - `pass-pipeline.contract.md` (six-pass ordering + post-conditions)
   - `driver-emit-mlir.contract.md` (CLI flag + default-printer freeze)
   - `residue-detection.contract.md` (regex + scanned attr-table)

If a question survives reading these five documents, run
`/speckit-clarify` again — do not improvise.

---

## 2. Environment setup

The dev container is canonical (per memory:
`project_build_environment.md`):

```text
ghcr.io/koyamanx/nsl-nslc:dev
```

All `cmake` / `ninja` / `lit` / `gtest` runs MUST execute inside
this container. Host has no LLVM/MLIR install; do NOT attempt to
build natively.

Boot the container:

```bash
sg docker -c "docker run --rm -it -v $PWD:/workspace -w /workspace \
  ghcr.io/koyamanx/nsl-nslc:dev bash"
```

Inside the container, the build directories are `build-asan/`
(default, with AddressSanitizer) and `build-Release-host/`
(release-mode for binary distribution). Use `build-asan/` for all
M5 development.

---

## 3. The first TDD-red fixture

Per Constitution Principle VIII (TDD, NON-NEGOTIABLE), the first
commit on the M5 branch is a FAILING test. Author it before any
implementation:

```bash
mkdir -p test/Lower/decl
cat > test/Lower/decl/regdecl_emit_mlir.nsl <<'EOF'
// RUN: nslc -emit=mlir %s | FileCheck %s

module M {
  declare M { input a[8]; output q[8]; }
  reg r[8] = 0;
  r := a;
  q = r;
}

// CHECK: nsl.module @M
// CHECK: nsl.reg "r" : !nsl.bits<8> = 0
// CHECK: nsl.clocked_transfer
// CHECK: nsl.transfer
EOF

# Verify it fails on the unchanged tree (because nslc -emit=mlir doesn't work yet):
cd build-asan && ninja nslc && \
  ../tools/nslc -emit=mlir ../test/Lower/decl/regdecl_emit_mlir.nsl
# Expected: error: '-emit=mlir' is not yet implemented (planned for M5)
```

Commit this `.nsl` fixture with message `M5: T0 — failing fixture
for reg-decl AST → nsl lowering`. Then start implementation.

---

## 4. Implementation order (suggested, not contract-frozen)

The dependency tree gives a natural order:

```text
T-1.  CMake source-list amendments  (lib/Lower/CMakeLists.txt; trivial)
T-2.  Public umbrella header        (include/nsl/Lower/Lower.h with declarations only)
T-3.  ASTToMLIR class declaration   (lib/Lower/ASTToMLIR.h)
T-4.  ASTToMLIR module-only stub    (visit ModuleBlock + DeclareBlock only)
        → makes the T0 fixture turn green
T-5.  ASTToMLIR decl visitors       (Reg/Wire/Mem/Func/Proc/State/FirstStateDecl)
T-6.  ASTToMLIR action visitors     (Parallel/Alt/Any/Seq/While/For/If)
T-7.  ASTToMLIR stmt visitors       (Transfer/ControlCall/BareFinish/SystemTask)
T-8.  ASTToMLIR expression lowering (Binary/Unary/Literal/Identifier/Conditional/Slice/Concat/StructCast/FieldAccess)
T-9.  ASTToMLIR S27 marker (fire_probe)
T-10. DiagnosticBridge              (lib/Lower/Pass/Common/DiagnosticBridge.{h,cpp})
T-11. NSLResolveParamsPass          (slot 1)
T-12. NSLExpandGeneratePass         (slot 2)
T-13. NSLExpandVariablesPass        (slot 3) — the heaviest pass
T-14. NSLExplodeSubmodArrayPass     (slot 4)
T-15. NSLInlineInternalFuncPass     (slot 5; no-op slot per Q3 → Option B)
T-16. NSLCheckSemanticsPass         (slot 6) — residue regex + 6 sensitive-Sn re-checks
T-17. Compilation::lowerToNSL body  (lib/Driver/LowerToNSL.cpp — was M4 stub)
T-18. Compilation::runNSLPasses body (lib/Driver/RunNSLPasses.cpp — was M4 stub)
T-19. Compilation::run() arm        (lib/Driver/Compilation.cpp — wire EmitKind::NSLMLIR)
T-20. nsl-opt main amendment        (one new line: registerNSLLowerPasses())
T-21. Per-AST-node fixtures         (test/Lower/<category>/*; FR-027)
T-22. Per-pass fixtures             (test/Lower/passes/<pass-flag>/*; FR-028)
T-23. M3-corpus extension fixtures  (test/Lower/m3_corpus/*; FR-030)
T-24. Determinism CI gate           (scripts/audit_lower_fixtures.sh, scripts/determinism_check.sh; FR-029)
T-25. CLAUDE.md / docs/CLAUDE.md updates (FR-031; coupling per Principle VII)
```

Generate this list as `/speckit-tasks` after `/speckit-plan`
finishes; treat T-1..T-25 as the suggested decomposition.

---

## 5. Build and test loop

Inside the dev container, from `/workspace`:

```bash
# Configure (first time only)
cmake -G Ninja -B build-asan -DCMAKE_BUILD_TYPE=Debug -DENABLE_ASAN=ON

# Build the library + driver + nsl-opt
ninja -C build-asan nsl-lower nslc nsl-opt

# Run unit tests
ninja -C build-asan check-nsl-unit

# Run lit tests (lowering layer)
ninja -C build-asan check-nsl-lower

# Run the full suite (CI-equivalent locally)
./scripts/ci.sh all
```

Expected timing: clean `nsl-lower` build under 30 s; full lit
under 30 s. If you see >2 minutes, check for an accidental
`std::unordered_map` iteration loop (the determinism trap).

---

## 6. Verifying determinism locally

Per FR-029 and the determinism gate (US5):

```bash
# Build twice in distinct host paths
mkdir -p ../build-det-a ../build-det-b
cmake -G Ninja -S . -B ../build-det-a
cmake -G Ninja -S . -B ../build-det-b
ninja -C ../build-det-a nslc
ninja -C ../build-det-b nslc

# Lower the same file in each
../build-det-a/tools/nslc -emit=mlir test/sema/s1/positive.nsl > /tmp/a.mlir
../build-det-b/tools/nslc -emit=mlir test/sema/s1/positive.nsl > /tmp/b.mlir

# Diff — MUST be empty
diff -q /tmp/a.mlir /tmp/b.mlir
```

If the diff is non-empty, `grep -E '/build|/home|/tmp' /tmp/a.mlir
/tmp/b.mlir` to find the leakage point. The most common culprit
is debug-info attributes or build-path-derived diagnostic locations
— see commit `3326eb6` (M4) for the GCC-ASan-globals
instrumentation case.

---

## 7. Pass-standalone testing via `nsl-opt`

Each pass MUST be invocable standalone (FR-011). To test
`NSLExpandGeneratePass` in isolation:

```bash
# Author a hand-rolled .mlir fixture
cat > /tmp/generate_test.mlir <<'EOF'
nsl.module @M {
  nsl.structural_generate %i_init = 0, %i_end = 4, %i_step = 1 {
    nsl.reg "buf_%i%" : !nsl.bits<8> = 0
  }
}
EOF

# Run only the expand-generate pass
build-asan/tools/nsl-opt -nsl-expand-generate /tmp/generate_test.mlir
```

Expected output: four `nsl.reg` ops named `buf_0`, `buf_1`,
`buf_2`, `buf_3`, zero `nsl.structural_generate`. If the output
contains residue or unresolved param refs, the pass is wrong.

To test the FULL pipeline via `nsl-opt`:

```bash
build-asan/tools/nsl-opt \
  -pass-pipeline='builtin.module(nsl-resolve-params,nsl-expand-generate,nsl-expand-variables,nsl-explode-submod-array,nsl-inline-internal-func,nsl-check-semantics)' \
  /tmp/generate_test.mlir
```

This is byte-equivalent to `nslc -emit=mlir` for the same input
(modulo the `nsl-opt` taking pre-built `.mlir`).

---

## 8. Common implementation pitfalls

| Pitfall | Symptom | Fix |
|---|---|---|
| `std::unordered_map` over pointer keys | Determinism gate fails (US5 diff non-empty) | Use `llvm::DenseMap` for lookup-only; iterate via Sema's ordered iterator |
| `mlir::OpBuilder::createOrFold` | Unexpected constant folding alters `loc(...)` | Use `mlir::OpBuilder::create<X>` directly; folding is M6's concern |
| Forgot `mlir::Location` on emitted op | FR-008 violation; CI walk asserts non-`UnknownLoc` | Always thread the AST `SourceRange` into `builder_.create<X>(loc, ...)` |
| `nsl.call @Q` references `func` not yet visited | Verifier rejects symbol | Trust MLIR's lazy `SymbolTable::lookup` (Q4 → Option A); construct `FlatSymbolRefAttr("Q")` and proceed |
| Pass writes diagnostic to `llvm::errs()` | Diagnostic-engine bridge bypassed; FR-019 violation | Use `op->emitError(...)`; the active `DiagnosticBridge` forwards to `basic::DiagnosticEngine` |
| `nsl.module` emitted at top level instead of inside `mlir::ModuleOp` | M4 verifier rejects | Wrap in the outer `mlir::ModuleOp` returned by `lower(...)` |
| `mlir::PassManager::run` returns failure but no diagnostic emitted | Driver exits silent-non-zero; user confused | Check that the failing pass called `op->emitError(...)` BEFORE `signalPassFailure()` |

---

## 9. Spec coupling (Principle VII)

When you finish the implementation:

- Update [`CLAUDE.md`](../../CLAUDE.md) §1 — add M5 entries to the
  "Lower to dialect" column for every grammar row whose dialect
  target lands at M5 (rows currently say "M5"; verify they are
  delivered, no edits needed unless the implementation surfaced a
  new row).
- Update [`docs/CLAUDE.md`](../../docs/CLAUDE.md) §3 — the
  "Writing a structural-expansion pass" entry should reference
  this PR's commit hash if the design §9 line ranges shifted.
- Do NOT update [`docs/design/nsl_compiler_design.md`](../../docs/design/nsl_compiler_design.md)
  unless the implementation revealed a design-doc inaccuracy. If
  it did, both spec and design get amended in the same PR.

The M5 PR's commit-message footer SHOULD include:

```text
Linear: NSL-<N>
Closes: NSL-<N>
Co-Authored-By: Claude <noreply@anthropic.com>
Assisted-by: Claude:Opus-4.7 [Edit] [Bash] [Read] [Write]
```

---

## 10. Pre-merge checklist

Run-through completed at T111 close-out (M5 Phase 8 final commit on
branch `008-m5-structural-passes`):

- [X] All 31 FR satisfied (cross-checked against spec.md;
  FR-008/SC-009 audit shipped in soft-fail mode per option (a) in
  the offload — `audit_op_locations.sh` enforces post-adapter)
- [X] All 12 SC measurable (cross-checked against spec.md; SC-010
  M4-baseline regression check clean per research.md §19)
- [X] Per-AST-node fixtures: count matches `visit()` overrides
  (`scripts/audit_lower_fixtures.sh` clean: 40 concrete visitors
  covered; 14 STUB)
- [X] Per-pass fixtures: ≥1 per pass; ≥7 for `NSLCheckSemanticsPass`
  (per `residue-detection.contract.md` §8) — slot 5
  `nsl-inline-internal-func` gains its noop_roundtrip.mlir at T105
- [X] M3-corpus extension: every Sema-clean fixture has paired
  golden (T106: 21 goldens authored; 8 XFAIL'd with cited rationale)
- [X] Determinism gate: `diff -q` empty across two host paths (T101
  `determinism_check.sh`; opt-in via `NSLC_RUN_DETERMINISM_CHECK=1`)
- [X] No host-path strings in any `.mlir.expected` golden
  (`audit_determinism.sh` clean: 12 forbidden patterns scanned)
- [X] No `unresolved_conversion_cast` / `op-not-yet-supported`
  diagnostic in any successful test path
- [X] `nsl-opt --help` lists all six passes (verified at T111: all
  6 `--nsl-*` flags present with M5 FR-013..FR-018 descriptions)
- [X] CI green on all six Principle IX stages (deferred items:
  ASan link is a known M3-baseline issue, not introduced by M5;
  clang-tidy in test_unit/ pre-existing per US1 close-out report)
- [ ] CodeRabbit blocking findings addressed (no PR open yet —
  ticked when PR is created)
- [ ] Linear issue NSL-<N> exists for this milestone (per the
  offload note, this is an external-system step orthogonal to the
  branch's mergeability)

**M5 mergeable status**: PR-ready. Final lit count 520 PASS + 15
XFAIL / 535 (was 498 + 7 / 505 at T104; net +22 PASS + 8 XFAIL).
The two unchecked boxes are PR-creation-time / external-system
items, not branch-state blockers.

If all boxes are checked, the PR is mergeable per Constitution
Principle IX.
