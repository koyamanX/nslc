<!-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception -->

# Implementation Plan: M6 — `nsl-lower` part 2 (`nsl` → CIRCT lowering)

**Branch**: `010-m6-circt-lowering` | **Date**: 2026-05-04 | **Spec**: [spec.md](./spec.md)
**Input**: Feature specification from `/specs/010-m6-circt-lowering/spec.md`

## Summary

M6 delivers the second half of the `nsl-lower` library (layer 8b
per `docs/design/nsl_compiler_design.md` §3 lines 132–148, §10
lines 1202–1267): the **`NSLToCIRCTPass` conversion pass** that
consumes the M5-frozen `nsl::*` IR and produces an `mlir::ModuleOp`
populated entirely by ops from CIRCT's `hw`, `comb`, `seq`, `fsm`,
and `sv` dialects. The user-visible deliverable is a working
`nslc -emit=hw input.nsl` driver flag whose output is byte-stable
across builds, verifier-clean under all five CIRCT dialects, and
survives the four stock CIRCT passes
(`convertFSMToSeq` → `lowerSeqToSV` → `prepareForEmission` →
`exportVerilog`-stripped) when invoked externally via `circt-opt`.
Headline sub-deliverables per README §Roadmap M6 row literal text:
"`nsl::ProcOp` / `nsl::StateOp` / `nsl::SeqOp` lower to
`fsm::MachineOp`" plus per-op coverage of every row in design §10's
mapping table (lines 1206–1258).

The technical approach (see [research.md](./research.md) for
Decision/Rationale/Alternatives per choice): MLIR
`DialectConversion` framework in **full conversion mode** with one
`OpConversionPattern<nsl::*>` subclass per design-§10 row;
`CIRCTTypeConverter` mapping `!nsl.bits<W>` → `iW` and
`!nsl.struct<@T>` → packed `iN` per S18 MSB-first ordering
(no `hwarith` types — Q1 → Option A from
[`spec.md`](./spec.md) §Clarifications). FSM lowering for
`nsl.proc` / `nsl.state` / `nsl.seq` produces `fsm::MachineOp`
with `initial_state` from `nsl.first_state` and per-`nsl.state`
`fsm::StateOp` children. Reg lowering uses `seq::FirRegOp` with
**async, active-low reset** (Q2 → Option C) by default; `seq::
CompRegOp` only on the explicit-`interface`-modifier (S20) path.
Conditional reg-update (`nsl.if` over reg LHS) lowers via
**mux-on-data** (Q3 → Option A): `seq::FirRegOp` data input
becomes `comb.mux(%cond, %new, %prev)`, regardless of conditional
nesting depth. Sim-only ops (`nsl.sim_*` plus the S29 `_init`
block) wrap in `sv.ifdef "SIMULATION"` per design line 1226.
Driver flag `-emit=hw` halts strictly at the conversion boundary
(Q2-specify-time → Option A); the four stock CIRCT passes are M7's
responsibility. New `Compilation::lowerToCIRCT` member function
routes failures through `basic::DiagnosticEngine` via the same
`DiagnosticBridge` RAII handler M5 introduced.

## Technical Context

**Language/Version**: C++17 across `nsl-lower` (Constitution
"Build, Code, and Licensing Standards"). C++20 features prohibited.
Matches M5's tightening.

**Primary Dependencies**: LLVM 18 + MLIR 18 + CIRCT (matched to
the `ghcr.io/koyamanx/nsl-nslc:dev` container's pinned versions —
M0 contract). M6 activates CIRCT dialects: `circt::hw`, `circt::
comb`, `circt::seq`, `circt::fsm`, `circt::sv` — one new
`LINK_LIBS` entry at M6 (`CIRCTFSM`); `CIRCTHW`, `CIRCTComb`,
`CIRCTSeq`, `CIRCTSV` were declared inert at M5 and become live
at M6 (no CMake change other than adding `CIRCTFSM`). MLIR's
`DialectConversion` framework (`mlir/Transforms/DialectConversion.h`)
is the conversion infrastructure.

**Storage**: N/A (compiler frontend; no persistent state).

**Testing**: lit + FileCheck (per Constitution Principle VI per-
layer accepted test driver: "Lowering tests use lit + FileCheck").
Test corpus organized under `test/Lower/circt/<family>/` with
families `module/`, `fsm/`, `arith/`, `state/`, `control/`,
`sim/`, `round_trip/`. Per-`nsl::*`-op fixtures (one `.nsl` +
one `.mlir.expected` per design-§10 row) plus per-US axis
fixtures. The CI coverage guard (FR-033) mechanically
enumerates registered conversion patterns and asserts each has
a matching fixture. gtest only for unit-level helpers (e.g.,
`CIRCTTypeConverter` width-arithmetic tests).

**Target Platform**: Linux x86_64 (Constitution Principle IX
build matrix); other platforms forward-looking. Dev container is
canonical (`ghcr.io/koyamanx/nsl-nslc:dev`).

**Project Type**: Compiler library + driver (single project,
LLVM-style layered architecture per Constitution Principle II).
M6 extends the `nsl-lower` library introduced at M5; no new
library, no new public umbrella header.

**Performance Goals**: M6 lit corpus completes in under 30 s in
CI (Principle IX stage 4 timing budget — same as M5). Per-fixture
conversion runtime under 100 ms wall-clock on the dev container's
typical 8-vCPU runner (informational, not a contract surface).
SC-001's "+25% wall-clock budget vs `nslc -emit=mlir`" is a soft
regression-tracking benchmark, NOT a CI gate (per the spec
Outstanding/Deferred summary).

**Constraints**: Determinism (Principle V) — every code path
producing a name MUST use stable iteration; the CI grep introduced
at M5 (no `std::unordered_*` / pointer-derived ordering / time
sources in `lib/Lower/`) extends to all M6 code paths. Single
public umbrella header (Principle II) — `nsl-lower` is NOT one of
the named exceptions for `nsl-ast` / `nsl-sema`; M6 re-exports
all new symbols from the existing `Lower.h`. Diagnostic plumbing
(Principle IV) — every CIRCT op carries `mlir::Location`
inherited from the source `nsl::*` op; every conversion-pattern
failure flows through `basic::DiagnosticEngine` via the
`DiagnosticBridge` RAII handler. Stock-CIRCT-below-the-`nsl`-
dialect (Principle III) — M6 writes ZERO hand-rolled CIRCT-
equivalent passes; every output op is a real `circt::*` op.

**Scale/Scope**: 2 new public symbols added to `Lower.h`
(`createNSLToCIRCTPass()` constructor + `registerNSLToCIRCTPass()`
registration helper) — total `Lower.h` surface grows from M5's 8
to **10** (count frozen by `lower-api.contract.md` §6 amendment
for M6). One new `Compilation::lowerToCIRCT` member function on
the driver. ~40 `OpConversionPattern` subclasses — one per design-
§10 mapping-table row — averaging ~30–80 LOC each. ~50–60 per-op
fixtures + ~15 axis fixtures (US2 module, US3 FSM, US5 round-trip)
+ ~10 sim-only fixtures = ~75–85 lit fixtures total at M6
acceptance.

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-check after Phase 1
design.*

### Phase 0 (pre-research) gate

| Principle | Status | Notes |
|---|---|---|
| I. Spec Is Authoritative | **Pass** | M6 introduces no new `Sn`/`Nn`/`Pn`. Every conversion pattern operationalises an existing constraint via the design-§10 mapping table; the table itself was authored at design time and frozen by M4's contract. The "no silent AST drops" sub-clause does not apply at this layer (M6 consumes M5's `nsl::*` IR; it does not parse). |
| II. Layered Library Architecture | **Pass** | M6 extends `nsl-lower` (layer 8b); depends on `nsl-sema` + `nsl-dialect` as M5 already declared. No new sibling deps. Single public umbrella header `Lower.h` (NOT a named exception); the 2 new symbols re-export from there. `nsl-opt` continues to reuse `libNSLFrontend.a` via the standard link path. |
| III. Stock CIRCT Below | **Pass** | This is the milestone where the seam first goes live. Zero hand-rolled CIRCT-equivalent passes — every output op is a real `circt::hw::*`, `circt::comb::*`, `circt::seq::*`, `circt::fsm::*`, or `circt::sv::*` op. The four stock CIRCT passes (`convertFSMToSeq` etc.) are NOT invoked from `nslc -emit=hw` per Q2-specify-time → A — they belong to M7's `-emit=verilog`. |
| IV. Source-Locating Diagnostics | **Pass** | FR-005 / FR-030 mandate `mlir::Location` inheritance on every CIRCT op (via `OpConversionPattern::rewriter.setInsertionPoint(...)` + manual `getLoc()` propagation, or `mlir::FusedLoc` aggregation when one `nsl::*` op produces multiple CIRCT ops). FR-028 routes conversion-pattern-match failures through `basic::DiagnosticEngine`. SC-004 asserts via CI grep guard. |
| V. Inspectable, Deterministic Pipeline | **Pass** | New `-emit=hw` flag (FR-023…FR-027) + determinism contract (FR-029, US5, SC-003). Reuses M5's MLIR-default-printer convention (FR-025) for byte-stable output. The CI determinism gate (M5's two-host-path `diff -q`) extends to `-emit=hw`. |
| VI. Layered Test Discipline | **Pass** | Lowering tests use lit + FileCheck per the per-layer accepted-driver clause; `test/Lower/circt/<family>/` family taxonomy organizes per design-§10 mapping rows. Audited corpus is M7, NOT M6 (acceptance scenarios use hand-authored representative samples; FR-032). The M3-corpus extension under `test/Lower/m3_corpus/` from M5 is unchanged at M6 — those fixtures stop at `-emit=mlir`. |
| VII. Spec ↔ Design Coupling | **Pass** | No `Sn`/`Nn`/`Pn` change at M6 → no quick-map / line-range update needed in `docs/CLAUDE.md` §5 quick-map. The cross-reference table in `docs/CLAUDE.md` §8 is unchanged. The Forward Roll-up in root `CLAUDE.md` §1 ("Lower to CIRCT" column) gains M6 entries via [`README.md`](../../README.md) §Roadmap M6 row already in place. The "Active feature" SPECKIT block in root `CLAUDE.md` is updated to point here. |
| VIII. Test-First Development | **Pass** | Each new conversion pattern lands with its lit fixture authored first (TDD). The CI coverage guard (FR-033) enforces fixture-pattern bijection mechanically. The constructive-`Sn` carve-out from v1.6.0 does not apply at M6 (M6 introduces no `Sn`). |
| IX. Continuous Integration & Delivery | **Pass** | M0's CI matrix (`./scripts/ci.sh all` six stages) absorbs M6 with no schema change — M6 fixtures slot under existing lit-test stage. The two-host-path determinism gate already exists; M6 extends it to `-emit=hw` outputs. |

### Phase 1 (post-design) gate

Re-evaluated after Phase 1 artefacts (research.md, data-model.md,
contracts/, quickstart.md) are written. The post-design status
is identical to Phase 0 above — no new violations surfaced during
artefact authoring; all decisions (Q1–Q3 from `## Clarifications`,
plus the planning-time decisions listed in
[`research.md`](./research.md) §§1–10) preserve the constitutional
invariants.

## Project Structure

### Documentation (this feature)

```text
specs/010-m6-circt-lowering/
├── plan.md                  # This file (/speckit-plan output)
├── spec.md                  # /speckit-specify + /speckit-clarify output
├── research.md              # Phase 0 output (this command)
├── data-model.md            # Phase 1 output (this command)
├── quickstart.md            # Phase 1 output (this command)
├── contracts/               # Phase 1 output (this command)
│   ├── lower-api.contract.md         # Extension to M5's contract
│   ├── circt-lowering.contract.md    # Per-op mapping (design §10) freeze
│   ├── driver-emit-hw.contract.md    # CLI flag freeze
│   └── firreg-convention.contract.md # Q2 + Q3 conventions freeze
├── checklists/
│   └── requirements.md      # /speckit-specify output (closed)
└── tasks.md                 # Phase 2 output (/speckit-tasks — NOT created here)
```

### Source Code (repository root)

```text
include/nsl/Lower/
└── Lower.h                  # Single public umbrella (M5-introduced).
                             #   M6 EXTENDS: adds 2 symbols
                             #   (createNSLToCIRCTPass +
                             #    registerNSLToCIRCTPass).

lib/Lower/
├── CMakeLists.txt           # add_nsl_library — M6 adds CIRCTFSM
│                            #   to LINK_LIBS.
├── ASTToMLIR.cpp            # M5 — unchanged at M6.
├── ASTToMLIR.h              # M5 — unchanged at M6.
├── DiagnosticBridge.cpp     # M5 — reused at M6 unchanged.
├── DiagnosticBridge.h       # M5 — reused.
├── NSLResolveParamsPass.cpp        # M5 — unchanged.
├── NSLExpandGeneratePass.cpp       # M5.
├── NSLExpandVariablesPass.cpp      # M5.
├── NSLExplodeSubmodArrayPass.cpp   # M5.
├── NSLInlineInternalFuncPass.cpp   # M5 (no-op slot).
├── NSLCheckSemanticsPass.cpp       # M5.
├── NSLToCIRCTPass.cpp       # NEW M6 — pass driver + pattern set
│                            #   registration. ~150 LOC.
├── NSLToCIRCTPass.h         # NEW M6 — private header.
├── CIRCTTypeConverter.cpp   # NEW M6 — TypeConverter
│                            #   implementation. ~60 LOC.
├── CIRCTTypeConverter.h     # NEW M6 — private header.
└── CIRCTPatterns/           # NEW M6 — one .cpp per pattern family.
    ├── ModulePatterns.cpp           # nsl.module, nsl.declare → hw.HWModuleOp
    ├── PortPatterns.cpp             # input/output/control terminals
    ├── StatePatterns.cpp            # nsl.reg, nsl.wire, nsl.mem,
    │                                #   nsl.transfer, nsl.clocked_transfer
    ├── ControlPatterns.cpp          # nsl.alt, nsl.any, nsl.if, nsl.call
    ├── FSMPatterns.cpp              # nsl.proc, nsl.state, nsl.seq,
    │                                #   nsl.first_state, nsl.goto, nsl.finish
    ├── ArithPatterns.cpp            # nsl.{add,sub,mul,…} → comb.*
    ├── BitOpPatterns.cpp            # nsl.{and,or,xor,shl,shr,reduce_*,
    │                                #   sign_extend,zero_extend,mux,concat,
    │                                #   extract,repeat} → comb.*
    ├── SimPatterns.cpp              # nsl.sim_* → sv.* under sv.ifdef
    └── ParamPatterns.cpp            # nsl.param_int, nsl.param_str,
                                     #   nsl.submodule

lib/Driver/
├── Compilation.cpp          # M5 — adds lowerToCIRCT() body.
└── Compilation.h            # M5 — adds lowerToCIRCT() declaration
                             #   + EmitKind::HW emission wiring.

test/Lower/circt/            # NEW M6.
├── module/                  # US2 fixtures (~10 .nsl + .mlir.expected pairs).
├── fsm/                     # US3 fixtures (~10 pairs).
├── arith/                   # US4 arithmetic fixtures (~15 pairs — one per design-§10 row 1227–1245).
├── state/                   # US4 state-element fixtures (~8 pairs — reg, wire, mem, transfer variants).
├── control/                 # US4 control-flow fixtures (~10 pairs — alt, any, if, call).
├── sim/                     # US4 sim-task fixtures (~6 pairs — sim_*, _init).
├── round_trip/              # US5 fixtures (~8 .nsl files; assertion is byte-stable + circt-opt-clean).
└── coverage_guard.cmake     # CI mechanism (FR-033).
```

**Structure Decision**: Single project, LLVM-style layered
architecture per Constitution Principle II. The `nsl-lower`
library M5 introduced is extended in place; M6 introduces the
`CIRCTPatterns/` sub-directory under `lib/Lower/` to keep the
~40 `OpConversionPattern` subclasses organized by op-family
(see Project Structure tree above). Each pattern family file is
~100–250 LOC; total new code under `lib/Lower/` at M6 is
~1500–2000 LOC. Test corpus growth is ~75–85 lit fixtures plus
the coverage guard. Public-header surface grows by 2 symbols on
the existing `Lower.h` umbrella — no new public header is added.

## Complexity Tracking

> **No Constitution Check violations**: Phase 0 and Phase 1 gates
> both pass without exception. Complexity tracking table omitted.

The single design choice that warrants explicit defence (per the
Phase 0 gate's "Pass" judgment with rationale) is the addition of
`CIRCTFSM` as a new `LINK_LIBS` entry. This is forced by Q3 → A
(mux-on-data data-path conditionals) coupled with the README §Roadmap
M6 row's literal "`fsm::MachineOp`" mention — the FSM dialect is
non-substitutable for the proc/state lowering. Discussed in
[research.md](./research.md) §3 (FSM-target choice).
