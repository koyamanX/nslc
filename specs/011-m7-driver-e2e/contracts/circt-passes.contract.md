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

## §1 Pass pipeline (frozen — amended 2026-05-12)

`Compilation::runCIRCTPasses` MUST invoke the following two
upstream-CIRCT passes in this exact order, via an un-anchored
`mlir::PassManager` (root-op-agnostic; see §1.1 for the reason
the PM is NOT nested at `mlir::ModuleOp`):

| Slot | Pass C++ factory | Pass-flag string | Owning lib | Effect |
|---|---|---|---|---|
| 1 | `circt::createConvertFSMToSVPass()` | `--convert-fsm-to-sv` | `CIRCTFSMToSV` | `fsm::MachineOp` / `fsm::StateOp` → state-register enum + comb-mux next-state logic |
| 2 | `circt::createLowerSeqToSVPass()` | `--lower-seq-to-sv` | `CIRCTSeqToSV` | `seq::FirRegOp` / `seq::CompRegOp` / `seq::FirMemOp` → `sv.reg` + `sv.alwaysff` + initial-value handling |

After slot 2 completes successfully + `circt::exportVerilog` (or
`exportSplitVerilog`) is invoked (which runs PrepareForEmission
internally per §1.1), the module is in emission-ready shape:
every reachable op belongs to `hw`, `comb`, or `sv` dialect.

## §1.1 Why PrepareForEmission is NOT invoked explicitly

The original 3-slot frozen pipeline planned at /speckit-plan time
included `circt::createPrepareForEmission()` (vendored namespace
flat `circt::`, NOT `circt::sv::`; no `Pass` suffix). At M7
implementation time, two facts surfaced via build verification
inside `:dev`:

1. **Upstream documentation states it runs internally.** Per
   `circt/Conversion/Passes.td:76` (vendored CIRCT): "*ExportVerilog
   internally runs PrepareForEmission*." Explicit project-side
   invocation is therefore redundant.
2. **Explicit invocation fails the PassManager root-op check.**
   `createPrepareForEmission` is declared as `Pass<"prepare-for-emission">`
   (no root-op binding); a ModuleOp-anchored `PassManager`
   rejects scheduling it with `'builtin.module' op trying to
   schedule pass 'PrepareForEmission' on an unsupported
   operation`. An un-anchored `PassManager` also rejects it
   because the pass has no advertised root op shape at all.

Implementation reality at `lib/Driver/RunCIRCTPasses.cpp` (post-
T031): the `mlir::PassManager` is constructed un-anchored
(`mlir::PassManager pm(&mlir_ctx_)` — no second argument); slots
1 + 2 are added; the pipeline runs. ExportVerilog (invoked later
from `nsl::driver::emitVerilog`) handles the prepare-for-emission
step internally.

## §2 Naming-drift note

The vendored CIRCT in `ghcr.io/koyamanX/nsl-nslc:dev` ships
`--convert-fsm-to-sv` and `--lower-seq-to-sv` as the pass-flag
strings, with factory functions in the flat `circt::` namespace
(NOT `circt::fsm::` / `circt::seq::` sub-namespaces). Design doc
`nsl_compiler_design.md` §10 historically named these as
`circt::fsm::convertFSMToSeq` / `circt::seq::lowerSeqToSV` — both
namespace-prefixed AND named under the post-rename FSM→Seq
convention. M7 follows the vendored-canonical reality
(`createConvertFSMToSVPass` + `createLowerSeqToSVPass` in flat
`circt::` namespace). The design doc was updated at
`docs/design/nsl_compiler_design.md` §10 in M7 Phase 7 polish
(commit `7a76a7f`).

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

ZERO custom passes are added by `nsl-driver` between slots 1–2
or before slot 1 or after slot 2. The pipeline is strictly the
two named upstream passes; `PrepareForEmission` runs internally
inside `circt::exportVerilog` per §1.1. Constitution Principle
III mandates this: every prep pass is `circt::*`; the project
does NOT reimplement or extend any of them.

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
