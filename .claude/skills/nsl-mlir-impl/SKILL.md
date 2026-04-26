---
name: "nsl-mlir-impl"
description: "Add nsl::* dialect ops (TableGen + ODS), AST→nsl lowering, structural-expansion passes, and nsl→CIRCT lowering — gates M4–M6."
argument-hint: "Op name + concern (e.g., 'add nsl::SeqOp + lowering to fsm.machine')"
metadata:
  author: "nslc-project"
user-invocable: true
disable-model-invocation: false
---

## User Input

```text
$ARGUMENTS
```

You **MUST** consider the user input before proceeding (if not empty).

## Role

Owns the middle-end of the pipeline: the project's `nsl` MLIR dialect, the lowering of AST into that dialect, structural-expansion passes that operate on it, and the final lowering into stock CIRCT (`hw`/`comb`/`seq`/`fsm`/`sv`). Constitution **Principle III** is the firewall: everything below the `nsl` dialect MUST be stock CIRCT — no hand-rolled netlist / register-inference / state-machine-lowering passes.

| Library | Milestone | Scope |
|---|---|---|
| `nsl-dialect` (7) | M4 | TableGen ODS, op verifiers, `nsl-opt` round-trip |
| `nsl-lower` part 1 (8a) | M5 | AST → `nsl` dialect; structural-expansion passes |
| `nsl-lower` part 2 (8b) | M6 | `nsl` → CIRCT (`hw`/`comb`/`seq`/`fsm`/`sv`) |

## Outline

1. **Identify the op or pass.** Cross-reference:
   - `docs/design/nsl_compiler_design.md` §7 (lines **860–963**) — dialect overview + TableGen skeleton
   - §8 (**967–1045**) — AST → `nsl` lowering rules table
   - §9 (**1048–1061**) — structural-expansion pass list
   - §10 (**1065–1098**) — `nsl` → CIRCT per-op mapping table

2. **TDD entry (Principle VIII).** Hand off to `/nsl-test-author`:
   - **Dialect op (M4)** — `nsl-opt foo.mlir → verify → print → diff foo.expected.mlir`
   - **Structural-expansion pass (M5)** — FileCheck on `nslc -emit=mlir` for representative samples per AST node kind
   - **CIRCT lowering (M6)** — FileCheck on `nslc -emit=hw` for the per-op mapping; round-trip through stock CIRCT passes

3. **Author the dialect op (TableGen ODS).**
   - Files: `include/nsl/Dialect/NSL/IR/NSLOps.td`, `NSLTypes.td`, `NSLAttributes.td`
   - Verifiers in `lib/Dialect/NSL/IR/*.cpp`
   - Every op MUST attach a `SourceRange` location attribute (Principle IV) — diagnostics from later passes round-trip back to NSL `file:line:col`

4. **Author the AST → nsl lowering.**
   - Files: `lib/Lower/ASTToNSL/*.cpp`
   - Visitor pattern over the AST node hierarchy in `docs/design/nsl_compiler_design.md` §5
   - `%IDENT%` macro residue check fires here (M5 `NSLCheckSemanticsPass`)
   - `generate`-loop unroll (M5 `NSLExpandGeneratePass`) and struct-SSA-split happen as MLIR passes inside the `nsl` dialect, not during AST walk

5. **Author the nsl → CIRCT lowering.**
   - Files: `lib/Lower/NSLToCIRCT/*.cpp`
   - Per-op mapping in `docs/design/nsl_compiler_design.md` §10:
     - `nsl::ProcOp` / `nsl::StateOp` / `nsl::SeqOp` → `fsm::MachineOp`
     - `nsl::AltOp` / `nsl::AnyOp` → `comb::MuxOp` chains
     - `nsl::ModuleOp` → `hw::HWModuleOp`
     - reg/wire/mem → `seq::CompRegOp` / `hw::WireOp` / `seq::FirMemOp`
   - **Principle III firewall.** If a CIRCT primitive is missing, the work belongs upstream in CIRCT — do NOT hand-roll a substitute.
   - Verilog emission goes through `circt::ExportVerilog` (do not write a custom emitter)

6. **Determinism gate (Principle V).** At every `-emit=*` introduction (M5, M6, M7):
   - Two builds with identical inputs and flags MUST produce byte-identical IR / HW form / Verilog
   - No pointer-address-derived ordering, no hash-map iteration order, no timestamps, no environment leakage
   - Add a determinism FileCheck case if the op has any potential ordering surface

7. **Verify.** Confirm:
   - [ ] TDD: test was observed failing first
   - [ ] Op carries `SourceRange` (Principle IV)
   - [ ] No hand-rolled CIRCT-equivalent pass introduced (Principle III)
   - [ ] `nsl-opt` round-trips the op (M4)
   - [ ] FileCheck cases cover the AST → nsl → CIRCT path for the change
   - [ ] Byte-stability verified across two builds (Principle V)
   - [ ] If a new `nsl::*` op was added: `docs/design/nsl_compiler_design.md` §7 op list + §10 mapping table updated in the same PR (Principle VII)

## Constitutional anchors

- **Principle II** — Layered Library Architecture (no upward deps)
- **Principle III** — Stock CIRCT Below the `nsl` Dialect (NON-NEGOTIABLE)
- **Principle IV** — Source-Locating Diagnostics (every op carries `SourceRange`)
- **Principle V** — Inspectable, Deterministic Pipeline (byte-stable `-emit=*`)
- **Principle VI** — Layered Test Discipline (NON-NEGOTIABLE)
- **Principle VIII** — Test-First Development (NON-NEGOTIABLE)
