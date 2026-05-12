<!-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception -->

# Contract: Stock-CIRCT pass pipeline (`Compilation::runCIRCTPasses`)

**Feature**: 011-m7-driver-e2e
**Owners**: `lib/Driver/RunCIRCTPasses.cpp`,
`lib/Driver/Compilation.cpp` (host)
**Status**: Frozen at M7
**Related FRs**: FR-002, FR-004
**Related contracts**: `driver-emit-verilog.contract.md` (consumer
of this pipeline's output)

---

## §1 Pass pipeline (frozen)

`Compilation::runCIRCTPasses` MUST invoke the following three
upstream-CIRCT passes in this exact order, via `mlir::PassManager`
nested at `mlir::ModuleOp` granularity:

| Slot | Pass C++ factory | Pass-flag string | Owning lib | Effect |
|---|---|---|---|---|
| 1 | `circt::fsm::createConvertFSMToSVPass()` | `--convert-fsm-to-sv` | `CIRCTFSMTransforms` | `fsm::MachineOp` / `fsm::StateOp` → state-register enum + comb-mux next-state logic |
| 2 | `circt::seq::createLowerSeqToSVPass()` | `--lower-seq-to-sv` | `CIRCTSeqTransforms` | `seq::FirRegOp` / `seq::CompRegOp` / `seq::FirMemOp` → `sv.reg` + `sv.alwaysff` + initial-value handling |
| 3 | `circt::sv::createPrepareForEmissionPass()` | `--prepare-for-emission` | `CIRCTSVTransforms` | Renames for SystemVerilog-identifier legality; insert `sv.assign` for `sv.wire` in legal positions; misc emission prep |

After slot 3 completes successfully, the module is in the
emission-ready shape: every reachable op belongs to `hw`, `comb`,
or `sv` dialect.

---

## §2 Naming-drift note

The vendored CIRCT in `ghcr.io/koyamanX/nsl-nslc:dev` ships
`--convert-fsm-to-sv` as the pass-flag string. Design doc
`nsl_compiler_design.md` §10 lines 1297–1302 historically named
the equivalent C++ entry `circt::fsm::convertFSMToSeq`. The
post-upstream-rename name is `convertFSMToSV`. M7 follows the
upstream reality (the renamed `convertFSMToSV` name); the design
doc is updated via a documentation retrospective post-M7 merge
(routine, not a constitutional change).

---

## §3 Pass manager configuration

The `mlir::PassManager` instance MUST:

- Be constructed against the existing `Compilation::mlirCtx_` (a
  single `mlir::MLIRContext` instance shared across the pipeline
  — established at M4).
- Be nested at `mlir::ModuleOp` (NOT function-level; all three
  passes operate on `ModuleOp`).
- Have parallel-mode DISABLED (`pm.enableMultithreading(false)`
  if needed) to preserve byte-determinism (Principle V).
- Carry a `mlir::ScopedDiagnosticHandler` for the duration of
  `pm.run(module)` so MLIR diagnostics route through
  `basic::DiagnosticEngine`.
- Have IR verification ENABLED between passes
  (`pm.enableVerifier(true)`) to catch any inter-pass verifier
  violation early — failures here flow through the diagnostic
  handler as well.

---

## §4 Failure semantics

`pm.run(module)` returns `mlir::LogicalResult`. M7 conventions:

- Success (`mlir::success()`) → return success to the caller; the
  module is now emit-ready.
- Failure (`mlir::failure()`) → at least one error-severity
  diagnostic was emitted via the scoped handler; the diagnostic
  is already recorded in `basic::DiagnosticEngine`. The caller
  (typically `Compilation::run`) checks `diag_.hasErrors()` and
  exits with code 1 per `driver-emit-verilog.contract.md` §2.

The pipeline does NOT auto-retry. A failure is fatal to the
current invocation; the user runs again after fixing input.

---

## §5 Verifier post-condition

After this pipeline completes successfully, `mlir::verify(module)`
MUST return success. Spot-check: no `nsl::*` op remains
(M6's full-conversion guaranteed that). After M7's pipeline: no
`fsm::*` or `seq::*` op remains (slots 1 + 2 lower them out).
Ops remaining: `hw::*`, `comb::*`, `sv::*` only.

The test gate `test/Driver/emit_verilog/passes_failure.test`
asserts that a hand-crafted module that survives M6 but trips
the verifier post-`runCIRCTPasses` (e.g., illegal `sv.reg` shape)
produces a clean diagnostic via the bridge.

---

## §6 No project-side pass insertion

ZERO custom passes are added by `nsl-driver` between slots 1–3
or before slot 1 or after slot 3. The pipeline is strictly the
three named upstream passes. Constitution Principle III mandates
this: every prep pass is `circt::*`; the project does NOT
reimplement or extend any of them.

If a future post-M7 milestone needs a project-side prep step, it
lands as a new `nsl-lower` pass invoked BEFORE
`Compilation::runCIRCTPasses` (i.e., before this contract's
boundary) — not inserted into the stock-CIRCT pipeline.

---

## §7 Forward compatibility

This contract is frozen at M7. Changes anticipated:

- CIRCT upstream may add additional emission-prep passes
  (e.g., a `--lower-comb-to-emit` follow-up). When that happens,
  this contract is amended to add a slot 4 — NOT in M7.
- A future milestone may add a `-O<level>` flag that toggles
  `--canonicalize` / `--cse` between slots. Currently NOT in
  scope.

The pass identity (factory function names + owning libs) is
considered upstream-vendor-canonical; if CIRCT renames a factory
post-M7, the contract is amended via a separate PR before the
rename ships in the vendored container.
