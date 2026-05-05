<!-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception -->

# Feature Specification: M6 — `nsl-lower` part 2 (8b): `nsl` → CIRCT lowering (`hw`/`comb`/`seq`/`fsm`/`sv`); `nslc -emit=hw` operational

**Feature Branch**: `010-m6-circt-lowering`
**Created**: 2026-05-04
**Status**: Draft
**Input**: User description: "M6: nsl → CIRCT lowering. Lower the M5-output `nsl` dialect to CIRCT's `hw`, `comb`, `seq`, `hwarith`, and `fsm` dialects so a complete NSL source compiles end-to-end to synthesizable structural IR. … New driver flag: `nslc -emit=core-mlir`. … Out of scope at M6: `nsl-driver` end-to-end Verilog emission (M7), audited-corpus regression (M7), formal verification (M8)."

> **Scope interpretation.** "M6" maps to the **M6** row of
> [`README.md`](../../README.md) §Roadmap, which delivers the next
> compiler-track library milestone after M5: `nsl-lower` part 2
> (layer 8b per [`docs/design/nsl_compiler_design.md`](../../docs/design/nsl_compiler_design.md)
> §3 lines 132–148, §10 lines 1202–1267) — the **`nsl` → CIRCT
> conversion pass** that consumes the M5-frozen `nsl::*` IR and
> produces a `mlir::ModuleOp` whose every reachable op belongs to one
> of the five CIRCT dialects: `hw`, `comb`, `seq`, `fsm`, `sv`. The
> README's M6 row defines the milestone's test gate ("**FileCheck on
> `nslc -emit=hw` for every per-op mapping in
> `nsl_compiler_design.md` §10; round-trip through stock CIRCT passes**")
> and its constitutional anchors (VI lowering tests; III stock CIRCT
> below the dialect; V determinism). The compiler-track table in
> [`CLAUDE.md`](../../CLAUDE.md) §1 confirms which language-spec rows
> land their "Lower to CIRCT" column entry at M6: every `nsl::*` op
> M4 froze gains a corresponding pattern in M6's `NSLToCIRCTPass`.
> M3 → M4 → M5 → M6 forms the single critical spine: M3 produced the
> typed AST + Sema observables, M4 produced the dialect IR shape +
> verifiers + `nsl-opt` round-trip, M5 turned the AST into IR and
> finished the structural-expansion phase, and M6 hands that IR off
> to CIRCT.
>
> **Driver-flag correction.** The user's input description named the
> flag `-emit=core-mlir`. The canonical name per the README's M6 row
> and design §11's `CompileOptions::EmitKind::HW` is **`-emit=hw`**
> (the M5 spec's "What does NOT land at M5" guard explicitly named
> `-emit=hw` as M6's deliverable, line 100). This spec uses the
> canonical name throughout.
>
> **What lands as a deliverable.** Three observable artefacts:
>
> 1. The `nsl-lower` static library (layer 8) gains its second body
>    of code: the `nsl::lower::NSLToCIRCTPass` (`mlir::OperationPass<
>    mlir::ModuleOp>`) plus its supporting `TypeConverter` and the
>    family of `mlir::ConversionPattern` subclasses (one pattern per
>    `nsl::*` op kind enumerated in design §10 lines 1206–1258). The
>    library's M0 `add_nsl_library` declaration in
>    `lib/Lower/CMakeLists.txt` (`DEPENDS nsl-sema nsl-dialect` +
>    `LINK_LIBS CIRCTHW CIRCTComb CIRCTSeq CIRCTSV CIRCTFSM` —
>    `CIRCTFSM` added at M6 if not already present at M5 — see Q1 → A
>    in `## Clarifications` for the no-`CIRCTHwArith` decision) is
>    updated. Per
>    Constitution Principle II's single-public-header rule, all M6
>    additions re-export from the existing `include/nsl/Lower/Lower.h`
>    umbrella header established at M5 — no new public header.
> 2. An `NSLToCIRCTPass` (`-nsl-to-circt` pass-name flag, registered
>    via the existing `Lower.h::registerPasses(...)` helper) usable
>    from `nsl-opt` standalone for hand-authored `.mlir` fixtures.
>    The pass uses MLIR's `DialectConversion` framework with a
>    full-conversion target (legal: `hw`, `comb`, `seq`, `fsm`, `sv`,
>    illegal: every op of the `nsl` dialect — see Q1 → A in
>    `## Clarifications` for the `comb`-only choice that excludes
>    `hwarith`).
>    Every op the conversion creates carries the originating
>    `nsl::*` op's `mlir::Location` (Constitution Principle IV —
>    diagnostic plumbing crosses the nsl → CIRCT seam intact).
> 3. A new driver flag `nslc -emit=hw` (per design §11
>    `CompileOptions::EmitKind::HW`) that halts the pipeline after
>    `runCIRCTLowering` and prints the final `mlir::ModuleOp` to
>    stdout (or to `-o <file>`) using MLIR's default printer with
>    `mlir::OpPrintingFlags()` (matching the M5 `-emit=mlir`
>    convention). Pipeline stages strictly before this gate
>    (`-emit=tokens`, `-emit=ast`, `-emit=mlir`) are unchanged from
>    M5; pipeline stages strictly after (`-emit=verilog`) remain
>    forward-looking (M7).
>
> Plus one driver-surface artefact wiring the library into `nslc`:
>
> 4. A new `Compilation::lowerToCIRCT` (`mlir::LogicalResult` return,
>    runs the conversion pass on the M5-output `mlir::ModuleOp`)
>    member function. The signature is added at M6 — it does NOT
>    appear in M5's `Compilation` shape — and is final on this
>    branch (no future-stage stubbing). Failures route through the
>    project's single `basic::DiagnosticEngine` (Constitution
>    Principle IV); MLIR's built-in conversion-failure diagnostic is
>    bridged through, not leaked, so users see one diagnostic
>    channel.
>
> **What does NOT land at M6.** No `circt::exportVerilog` invocation
> and no `-emit=verilog` wiring — those are M7. No invocation of the
> stock CIRCT pipeline post-lowering (`circt::fsm::convertFSMToSeq`,
> `circt::seq::lowerSeqToSV`, `circt::sv::prepareForEmission`) —
> those passes run inside M7's pipeline, not inside `-emit=hw`. The
> M6 acceptance gate verifies that M6's output IR survives those
> passes when externally invoked (the README's "round-trip through
> stock CIRCT passes" gate), but `-emit=hw` itself stops at the
> nsl→CIRCT boundary. No `nsl-driver` (M7), no audited-corpus
> regression — the M7 row owns `P-VEN`/`P-VCD` deliverables. No
> formal verification (M8). No release artefacts (M9). The audited
> NSL projects (`cpu16`, `mips32_single_cycle`, `ahb_lite_nsl`,
> `mmcspi`, `SDRAM_Controler`, `rv32x_dev`, `turboV`) are NOT
> required to compile end-to-end at M6 — that is M7's gate. M6's
> test corpus is hand-authored representative samples per `nsl::*`
> op kind, organized to cover every row of design §10's mapping
> table.

## Clarifications

### Session 2026-05-04

- Q: How does `nsl.if` over a `reg` LHS materialize "conditional reg-enable" in the output IR (design line 1217 leaves this under-specified given `seq.firreg` has no enable port)? → A: **Option A — mux-on-data.** `nsl.if (%cond) { %r := %new; }` lowers to `seq.firreg %r, %m_clock, comb.mux(%cond, %new, %r), reset async %p_reset, value = <init>` (Q2 → C async-active-HIGH convention applies to the reset-port wiring; PR #14 Round-1 review correction superseded the original active-LOW `clk`/`rst_n` narrative). The register updates every clock cycle; conditional logic lives entirely in the data path. The lowered IR contains exactly one `seq.firreg` per `nsl.reg` regardless of how many enclosing `nsl.if`s the reg participates in — chained `nsl.if` becomes nested `comb.mux`. The `prev` arm of the mux is the firreg's OWN result (`%r`), so an unwritten register holds its previous-cycle value (PR #14 Round-1 Finding #9 fix; pre-fix the prev arm was the reset constant which fed the initialiser back every cycle). `seq.compreg` is used only on the explicit-`interface`-modifier path (FR-008 unchanged). `seq.clock_gate` is NOT introduced at M6 (no ICG-cell synthesis dependency). Rationale: universal FPGA + ASIC portability without depending on clock-gating cell availability; every clock edge is a real edge so VCDs match the audited corpus's reference-Verilog one-for-one (M7 golden-VCD gate stays satisfiable); it preserves the Q2-default `seq.firreg` path uniformly without introducing a compreg branch. Option B (switch to `seq.compreg` on conditional) was rejected because it forks FR-008 and creates two reset-attribute pathways for the same `nsl.reg`. Option C (clock-gating) was rejected because FPGA portability of inferred clock gating varies across tools and would require a per-target probe.
- Q: Reset polarity / synchronicity convention for `seq.firreg` (modules without an explicit `interface` modifier)? → A: **Option C — async reset.** Synchronicity: `seq.firreg`'s `async_reset` flag is set. Naming + polarity (PR #14 Round-1 review correction, 2026-05-04): the implicit ports use the NSL-spec-reserved names `m_clock` (auto-synthesized clock; nsl_lang.ebnf §15 line 818) and `p_reset` (auto-synthesized reset; nsl_lang.ebnf §15 line 820). The `p_` prefix indicates **active-HIGH** polarity (the straightforward reading of the prefix; an `n_reset` would be active-LOW). The `seq.firreg`'s reset operand is wired DIRECTLY to the `%p_reset` block-arg with NO `comb.icmp eq …, 0` adapter. The reset value comes from the `nsl.reg`'s initializer (`reg r[8] = K;` → reset value `K`; bare `reg r[8];` → reset value zero per S2/S23). When the user writes the `interface` modifier explicitly (S20), their declared clock/reset names + polarities are honored verbatim via `seq.compreg` (FR-008 unchanged). Rationale: targeted at both ASIC and FPGA flows; async reset matches the convention of the audited NSL corpus's reference Verilog. The original spec narrative (pre-PR-#14-Round-1) said `clk` / `rst_n` (active-LOW), but those names are NOT NSL-reserved keywords; the `m_clock` / `p_reset` pair is — that violation was caught in code review and corrected. Sync reset (Option A) would force the audited corpus's golden VCDs to retime on the boundary cycle, breaking M7's equivalence gate. Per-module config (Option D) was rejected because the absence of an `interface` modifier means the user didn't name a polarity — a project-wide default is the only well-defined behavior.
- Q: Arithmetic dialect strategy — `comb` only, or mixed `comb`/`hwarith`? → A: **Option A — `comb` only.** Every `nsl.{add,sub,mul,and,or,xor,shl,shr,land,lor,not,neg,lnot,reduce_*,sign_extend,zero_extend,mux,concat,extract,repeat}` op lowers to a corresponding `comb.*` op. Signedness is encoded per-op (`comb.icmp slt` for signed compare, `comb.icmp ult` for unsigned, etc.) — there is no signed-vs-unsigned operand-type encoding. `nsl.sign_extend` lowers to `comb.concat (replicate MSB-bit, operand)`; `nsl.zero_extend` to `comb.concat (zeros, operand)` (design lines 1249–1250 first form, NOT the parenthetical `hwarith.cast` form). No `CIRCTHwArith` link dependency is added at M6. Rationale: NSL surfaces unsigned/2's-complement bitvector arithmetic; `hwarith`'s signed/unsigned type system has no NSL counterpart. `comb`-only keeps M6 fixtures stable across CIRCT version bumps; `hwarith` adoption can be a future incremental refactor without amending this spec. Option B (mixed) was rejected because per-op-family branching doubles the test-fixture authoring surface for no observable user benefit at M6. Option C (`hwarith` first, `comb` fallback) was rejected because it makes per-op coverage non-deterministic across CIRCT versions. Option D (defer to plan.md) was rejected because the choice materially shapes every fixture under `test/Lower/circt/arith/`.

## User Scenarios & Testing *(mandatory)*

### User Story 1 — `nslc -emit=hw` produces verified CIRCT IR for every `nsl::*` op (Priority: P1)

A contributor authors a representative `.nsl` source file (or
selects one from the M3/M5 corpus), runs `nslc -emit=hw input.nsl
-o output.mlir`, and observes that: (1) the command exits zero with
no diagnostics, (2) the output file contains zero `nsl::*` ops —
every dialect op has been replaced by its CIRCT-equivalent per
design §10's mapping table, (3) the output's reachable ops belong
exclusively to `hw`, `comb`, `seq`, `fsm`, `sv` (Q1 → A:
no `hwarith`), (4) every CIRCT op carries an `mlir::Location` that resolves
to the original `.nsl` source position, and (5) `nsl-opt -verify-each`
on the output reports success (CIRCT verifiers are happy).

**Why this priority**: This **is** the M6 acceptance gate per the
README's M6 row literal text — "`-emit=hw`" is the user-visible
deliverable; `NSLToCIRCTPass` is the engine that makes it work.
Without `-emit=hw`, M7 (end-to-end Verilog) has no plumbing for its
last conversion stage, the four stock CIRCT passes (`convertFSMToSeq`
→ `lowerSeqToSV` → `prepareForEmission` → `exportVerilog`) have no
input IR to consume, and the dialect delivered at M4–M5 has no
consumer. Constitution Principle VI names "**Lowering tests** use
`nslc -emit=mlir` (or `-emit=hw`) for FileCheck-style verification"
as the layer's canonical test driver. P1 because every downstream
milestone is gated on this stage being operational.

**Independent Test**: Build the `nsl-lower` library + `nslc` driver.
For every row of design §10's mapping table (lines 1206–1258 — the
exhaustive enumeration of every `nsl::*` op's CIRCT equivalent),
ship `test/Lower/circt/<op>_emit_hw.nsl` plus its
`<op>_emit_hw.mlir.expected` golden — assert via lit + FileCheck
that `nslc -emit=hw %s` produces output containing the CIRCT op
named in the mapping table and zero ops in the `nsl` dialect. The
CI guard mechanically enumerates every conversion pattern
registered in `NSLToCIRCTPass` and asserts a matching fixture
exists. Independent of US2 (module skeleton — that is the entry
point but US1 covers it via shape verification), US3 (FSM
lowering — exercised by `nsl.proc`/`nsl.state` rows), US4
(combinational + state ops), and US5 (round-trip).

**Acceptance Scenarios**:

1. **Given** an NSL source `module M { declare M { input a[8]; output q[8]; } reg r[8] = 0; r := a; q = r; }`, **When** `nslc -emit=hw M.nsl` runs, **Then** stdout contains an `hw.module @M(%a: i8) -> (q: i8)`, an `seq.firreg` (lowered from `nsl.clocked_transfer`), no `nsl::*` ops, and every op's `loc(...)` resolves to `M.nsl:<line>:<col>`.
2. **Given** any input that compiles cleanly through M5 `runNSLPasses`, **When** `nslc -emit=hw` runs, **Then** the run is also diagnostic-free at M6 — Sema (M3) + structural-expansion (M5) is the gate; M6 does not introduce new diagnostics on M5-clean input. Conversion failures (e.g., a pattern firing on a malformed M5 output) route through `basic::DiagnosticEngine` and exit non-zero.
3. **Given** an output `.mlir` file produced by `nslc -emit=hw`, **When** that file is fed to `nsl-opt - -verify-each`, **Then** every CIRCT verifier (`hw`, `comb`, `seq`, `fsm`, `sv`) returns success. When fed to `nsl-opt -`, **Then** the round-trip is a fixed point (byte-identical second-pass output — the determinism gate).
4. **Given** an NSL source that exercises every reachable `nsl::*` op via the M5 corpus, **When** `nslc -emit=hw` runs over the corpus, **Then** the post-pass IR contains zero `nsl::*` ops AND the legal-op set is a subset of the five CIRCT dialects.
5. **Given** any `mlir::Location` on any `nsl::*` op in M5's output, **When** the conversion pattern fires, **Then** every CIRCT op the pattern creates inherits that `Location` (or a `FusedLoc` aggregating multiple sources when one `nsl::*` op lowers to multiple CIRCT ops).
6. **Given** a piped invocation `cat input.nsl | nslc -emit=hw -`, **When** the driver runs, **Then** the output goes to stdout and the exit code is zero on success / non-zero on diagnostic failure (matches `-emit=mlir` from M5).

---

### User Story 2 — `nsl.module` with its `nsl.declare` ports lowers to `hw.module` (Priority: P1)

A contributor authors an NSL source containing `declare M { input a[8]; output q[8]; clk_terminal c; reset_terminal r; }` paired with a `module M { … }`. After `nslc -emit=hw`, the output IR contains an `hw.module @M(...) -> (...)` whose port list reflects the `declare` directionality: `input`/`func_in` arguments become `hw.module` inputs, `output` arguments become `hw.module` results, and control terminals (`clk_terminal`, `reset_terminal`) are surfaced per the design §10 row for `nsl.module`. The `interface` modifier (S20) — when present — drives explicit clock/reset port wiring; when absent, an implicit clock port follows the project's M4-frozen default. Submodule references (`nsl.submodule`) lower to `hw.instance` per design §10 line 1212.

**Why this priority**: The module skeleton is the structural
entry point — without it, no other CIRCT op can be reached because
`hw.module`'s body region is the only legal parent for the rest of
the lowered IR. Without this user story, no other M6 user story
exercises end-to-end. P1 because every other US's acceptance
fixture requires a containing `hw.module`.

**Independent Test**: Ship `test/Lower/circt/module/<scenario>.nsl`
+ `<scenario>.mlir.expected` per axis: (a) zero-port module (no
`declare` — interface-less), (b) input-only / output-only / mixed
ports, (c) `interface` modifier present (explicit clk/rst per
S20), (d) `interface` modifier absent (implicit clk per project
convention), (e) submodule instantiation (`nsl.submodule` →
`hw.instance`), (f) parameter passing per S16 (`param_int` →
`hw.instance` parameter wire — design line 1255). Standalone path
uses `nsl-opt -nsl-to-circt` on a hand-authored `.mlir` fixture.
Independent of US3/US4/US5.

**Acceptance Scenarios**:

1. **Given** `declare M { input a[8]; output q[8]; }` paired with `module M { … }`, **When** `nslc -emit=hw M.nsl` runs, **Then** the output contains exactly one `hw.module @M(%a: i8) -> (q: i8)` with the body region populated by the conversion of the `module M`'s body.
2. **Given** a `declare M { interface clk_a, rst_b; … }` (S20 explicit interface modifier), **When** the conversion runs, **Then** the resulting `hw.module` has explicit clock/reset port arguments named `clk_a` and `rst_b`, and downstream `seq.firreg` ops reference them by name.
3. **Given** a submodule `module M { Sub sub; }` where `Sub` is also a `module`, **When** the conversion runs, **Then** the body of `hw.module @M` contains an `hw.instance "sub" @Sub(...)` op whose ports are wired per the `Sub` declare interface.
4. **Given** a top-level `param_int @N = 8;` consumed by an HDL submodule per S16, **When** the conversion runs, **Then** the corresponding `hw.instance` carries the parameter as a parameter wire (design line 1255).
5. **Given** an `nsl.module` with no `declare` block (port-less), **When** the conversion runs, **Then** the resulting `hw.module @M()` has empty input and output port lists, and the body is still well-formed.

---

### User Story 3 — `nsl.proc` / `nsl.state` / `nsl.seq` lower to `fsm.machine` (Priority: P1)

A contributor authors an NSL source containing a `proc P { state s0 { goto s1; } state s1 { … } }` (with paired `first_state s0;`) or a `seq { … }` block inside a `func`. After `nslc -emit=hw`, the corresponding region has been replaced by an `fsm.machine` whose `initial_state` attribute references the `nsl.first_state` target, whose `fsm.state` children correspond one-to-one with the `nsl.state` ops, and whose `nsl.goto` ops have been replaced by `fsm.transition` ops connecting the source and target states. For `nsl.seq` inside a `func`, the auto-generated state names follow the `seq_N` pattern (design §10 line 1219). `nsl.finish` / `nsl.finish_method` lower to `fsm.transition` to a sink state (design line 1223). `nsl.call` to a `proc_name` lowers to `fsm.transition` to the target proc's initial state (design line 1225).

**Why this priority**: FSM lowering is the README's named M6
pattern — the M6 row literal text explicitly calls out
"`nsl::ProcOp` / `nsl::StateOp` / `nsl::SeqOp` lower to
`fsm::MachineOp`". This is the most semantics-shifting conversion
in M6 (the only one where multiple `nsl::*` ops aggregate into a
single CIRCT op). Without it, the entire `proc`/`state`/`seq`
lattice that NSL programmers depend on cannot reach Verilog. P1
because the M7 audited-corpus regression cannot start to pass
without it (six of the seven audited projects use `proc`/`state`).

**Independent Test**: Ship
`test/Lower/circt/fsm/<scenario>.nsl` per axis: (a)
single-state proc (degenerate FSM, one state), (b) two-state proc
with `goto` (smallest non-trivial FSM), (c) proc with
`first_state` not first in source order, (d) `nsl.finish` inside a
proc body (transition-to-sink-state), (e) `nsl.seq` inside a
`func` (auto-generated `seq_N` state names), (f)
`nsl.call`-to-proc (cross-machine transition). Standalone path
uses `nsl-opt -nsl-to-circt` on hand-authored `.mlir` fixtures.
Independent of US2 (containing `hw.module` is assumed).

**Acceptance Scenarios**:

1. **Given** an `nsl.proc @P` containing `nsl.first_state @s0`, two `nsl.state` ops `@s0` and `@s1`, and an `nsl.goto @s1` inside `@s0`, **When** the conversion runs, **Then** the result is an `fsm.machine @P` with `initial_state = @s0`, two `fsm.state` children (`@s0`, `@s1`), and an `fsm.transition` from `@s0` to `@s1`.
2. **Given** an `nsl.seq` block inside an `nsl.func`, **When** the conversion runs, **Then** the result is an `fsm.machine` whose state names are `seq_0`, `seq_1`, … in source order, AND every `nsl.goto` to a label inside the seq lowers to `fsm.transition` to the corresponding `seq_N`.
3. **Given** an `nsl.finish` op inside a proc body, **When** the conversion runs, **Then** the resulting `fsm.machine` has a synthetic sink state, and the `fsm.transition` from the finish-op's enclosing state targets that sink (design line 1223).
4. **Given** an `nsl.call @Q` where `@Q` is a `proc_name`, **When** the conversion runs, **Then** the call lowers to an `fsm.transition` whose target is `@Q`'s initial state (design line 1225).
5. **Given** an FSM whose `nsl.first_state` declaration appears later in the source than `state` definitions (S28 permits this), **When** the conversion runs, **Then** the resulting `fsm.machine` still carries the correct `initial_state` attribute regardless of source ordering.

---

### User Story 4 — Combinational, state, and simulation `nsl::*` ops lower to `comb` / `seq` / `sv` (Priority: P2)

A contributor authors an NSL source exercising the leaf-op
patterns: arithmetic / bit-ops (`+`, `-`, `*`, `&`, `|`, `^`,
`<<`, `>>`), comparisons (`==`, `<`, `>=`, …), reductions, sign /
zero extension, slice (`a[3:0]`), concat (`{a,b}`), conditional
(`a ? b : c`), state elements (`reg`, `wire`, `mem`), transfers
(`=`, `:=`), and simulation system tasks (`_display`, `_finish`,
`_init`, `_delay`). After `nslc -emit=hw`, every leaf-op pattern
has fired per design §10's mapping table:

- `nsl.{add,sub,mul,and,or,xor,shl,shr,eq,ne,lt,le,gt,ge,land,lor,not,neg,lnot}` → corresponding `comb.*` op (design lines 1227–1245).
- `nsl.{reduce_and,reduce_or,reduce_xor}` → `comb.icmp eq`/`comb.icmp ne`/`comb.parity` (lines 1246–1248).
- `nsl.{sign_extend,zero_extend,mux,concat,extract,repeat}` → `comb.{concat,mux,extract,replicate}` patterns (lines 1249–1254).
- `nsl.reg` → `seq.firreg` (or `seq.compreg` if clock/reset are explicit per S20 interface modifier; design line 1209).
- `nsl.wire` → `hw.wire` (line 1210).
- `nsl.mem` → `seq.firmem` (line 1211).
- `nsl.transfer` (combinational) → direct value substitution via `comb` ops (line 1213).
- `nsl.clocked_transfer` → `seq.firreg` write (line 1214).
- `nsl.alt` → priority-encoded nested `comb.mux` chain (line 1215, S13 priority semantics).
- `nsl.any` → per-target `comb.or` of all matching cases (line 1216, S13 parallel semantics).
- `nsl.if` (statement) → `comb.mux` for wire LHS; conditional reg-enable for reg LHS (line 1217).
- `nsl.sim_display` / `nsl.sim_finish` / etc. → `sv.fwrite` / `sv.finish` / etc., guarded by `sv.ifdef "SIMULATION"` (line 1226).

**Why this priority**: This is the bulk-volume conversion work —
roughly 35 of design §10's 40+ mapping rows are leaf-op rewrites
that fire pattern-by-pattern under MLIR's `DialectConversion`. P2
(rather than P1) because the M6 acceptance gate is the
`-emit=hw` round-trip per US1, and US1's per-op fixture set
mechanically enumerates each row of the design-§10 table — so
this US is what US1 *is*, decomposed by op family. Splitting it
out clarifies which family is involved when a pattern fails or a
new op needs adding.

**Independent Test**: Ship
`test/Lower/circt/<family>/<op>.nsl` for each of the four leaf
families: `arith/`, `state/`, `control/`, `sim/`. Each fixture
exercises one design-§10-table row with a minimal containing
`hw.module`. Lit + FileCheck assertions verify the post-pass IR
contains the named CIRCT op and zero ops of the corresponding
`nsl` op family. Standalone path uses `nsl-opt -nsl-to-circt`
on hand-authored `.mlir` fixtures. Independent of US3 (FSM
lowering is its own US).

**Acceptance Scenarios**:

1. **Given** `nsl.add %a, %b : i8`, **When** the conversion runs, **Then** the result is `comb.add %a, %b : i8`.
2. **Given** `nsl.reg "r" : !nsl.bits<8> = 0` and an `nsl.clocked_transfer` writing it, **When** the conversion runs and the enclosing module has no explicit `interface` modifier, **Then** the result is `seq.firreg` with **async** reset wired to the module-level `p_reset` block-arg directly (active-HIGH per the NSL-reserved `p_` prefix; PR #14 Round-1 review correction) and reset value `0` (Q2 → C). With explicit `interface` (S20), **Then** the result is `seq.compreg` with the explicit clock/reset operands and the user's declared polarity.
3. **Given** `nsl.alt { case A: x; case B: y; default: z; }`, **When** the conversion runs, **Then** the result is a nested `comb.mux` chain whose priority order matches the source order (S13 priority semantics).
4. **Given** `nsl.any { case A: x; case B: y; }`, **When** the conversion runs, **Then** the result is `comb.or %case_A_value, %case_B_value` per S13 parallel semantics (line 1216).
5. **Given** `nsl.sim_display "value=%d", %v`, **When** the conversion runs, **Then** the result is an `sv.fwrite` op wrapped in an `sv.ifdef "SIMULATION"` block (design line 1226).
6. **Given** `nsl.mem "m" : !nsl.bits<8> [256]`, **When** the conversion runs, **Then** the result is `seq.firmem` with depth 256 and width 8.
7. **Given** `nsl.sign_extend %a : i4 to i8`, **When** the conversion runs, **Then** the result is `comb.concat (replicate %a[3] × 4, %a)` per design line 1249 (the `comb`-only path, fixed by Q1 → A).
8. **Given** `nsl.reg "r" : !nsl.bits<8> = 0;` and `nsl.if (%cond) { %r := %new; }`, **When** the conversion runs (no `interface` modifier on the enclosing module), **Then** the result is exactly one `seq.firreg %r, %m_clock, comb.mux(%cond, %new, %r), reset async %p_reset, value = 0` (Q3 → A mux-on-data, plus Q2 → C async-active-HIGH reset; PR #14 Round-1 correction). Two chained `nsl.if`s become nested `comb.mux`; one `seq.firreg` regardless. The mux's "false" arm is the firreg's OWN result (`%r`), so an unwritten reg holds its previous-cycle value (Round-1 Finding #9 fix).

---

### User Story 5 — Lowered IR survives stock CIRCT passes (round-trip determinism gate) (Priority: P2)

A contributor takes the output of `nslc -emit=hw input.nsl -o
output.mlir`, then invokes `circt-opt --convert-fsm-to-sv
--lower-seq-to-sv output.mlir` (the two stock CIRCT passes
applicable at the M6 boundary). **PR #14 review-#15 fix
(2026-05-05)**: this section originally specified
`--convert-fsm-to-seq --lower-seq-to-sv --prepare-for-emission`
to match design §10 lines 1262–1264; the actual M6 round-trip
uses the corrected recipe because (a) the vendored CIRCT in the
dev container ships `--convert-fsm-to-sv` (FSM lowers directly
to SV) but not `--convert-fsm-to-seq`, and (b)
`--prepare-for-emission` requires its input root to be
`hw.module`, but `nslc -emit=hw` emits a `builtin.module` with
`hw.module` children — `prepare-for-emission` is therefore M7
territory (where `circt::ExportVerilog` performs the root-op
extraction implicitly). The pass sequence terminates without
diagnostics, every CIRCT op verifier returns success, and
re-running `nslc -emit=hw input.nsl` produces byte-identical
output (the determinism gate).

**Why this priority**: This **is** the second clause of the
README's M6 test gate ("**round-trip through stock CIRCT passes**").
It pins down what "the lowering is correct" means operationally —
not just verifier-clean at the conversion boundary, but
robust under the canonical CIRCT pipeline that M7 invokes. P2
because it is conditional on US1–US4 producing correct shapes; if
US1's per-op gates pass, US5's round-trip almost certainly passes
too. Listing it separately makes the determinism + CIRCT-pass-
survival property an explicit testable observable.

**Independent Test**: Ship `test/Lower/circt/round_trip/`
fixtures consisting of `.nsl` + `.expected.v` pairs. The lit
recipe runs `nslc -emit=hw %s | circt-opt --convert-fsm-to-sv
--lower-seq-to-sv` (corrected from the design-§10-derived
"`--convert-fsm-to-seq … --prepare-for-emission`" recipe per
review-#15 fix above), asserts zero diagnostics on stderr, and
asserts byte-identical output across two consecutive
`nslc -emit=hw %s` invocations. Determinism
applies to: file-list permutation invariance (within a single
input), include-search-path order independence, and time / ASLR /
PID independence (Constitution Principle V). Independent of US1–
US4 in that the gate is on overall output, not per-op.

**Acceptance Scenarios**:

1. **Given** any `.nsl` file in `test/Lower/circt/round_trip/`, **When** `nslc -emit=hw %s` runs twice in succession, **Then** the two outputs are byte-identical (`diff` returns empty).
2. **Given** the output of `nslc -emit=hw input.nsl`, **When** that output is piped to `circt-opt --convert-fsm-to-sv --lower-seq-to-sv` (corrected recipe per review-#15 fix; dropped `--convert-fsm-to-seq` and `--prepare-for-emission`), **Then** every named pass terminates with `mlir::success()` and produces output free of `unrealized_conversion_cast` ops.
3. **Given** a file-list containing the same N inputs in two different orders, **When** `nslc -emit=hw input1.nsl input2.nsl ...` runs in each order, **Then** the per-module outputs (when sorted by module name) are byte-identical (file-list permutation invariance per the M5 determinism contract).
4. **Given** an environment with randomized ASLR / process ID, **When** `nslc -emit=hw` runs, **Then** no `loc(...)` attribute, no MLIR-internal pointer value, no random component leaks into the printer output (already inherited from M5's determinism gate; M6 does not regress).

---

### Edge Cases

- **Module without explicit `interface` modifier — implicit clock/reset wiring**: When `declare M { … }` does not include the S20 `interface` modifier, M6 uses implicit module-level signals named `m_clock` (positive-edge-triggered) and `p_reset` (async, active-HIGH) per Q2 → C corrected by PR #14 Round-1 review. The names come from `nsl_lang.ebnf` §15 lines 818, 820 (NSL-reserved keywords). Every `seq.firreg` in the module's body wires its reset port directly to the `p_reset` block-arg (no `comb.icmp` adapter). Modules using non-standard names (`CLK`, `RESET`, `nreset`, `clk`, `rst_n`, …) require an explicit `interface` modifier per S20 — no auto-renaming is attempted.
- **`_init` block (S29) lowering target**: NSL sources may contain an `_init { … }` block whose statements run once at simulation start (S29 permits placement at module top-level). The design §10 mapping table does NOT explicitly list a CIRCT target for `_init`. **Decision (specify-time clarification, 2026-05-04)**: `_init` lowers to a single `sv.initial { … }` op containing the conversion of every assignment in the block, the whole `sv.initial` wrapped in an `sv.ifdef "SIMULATION"` guard — same machinery as `nsl.sim_*` per design line 1226. Simulation-time semantics are preserved; synthesis tools elide the `ifdef`. Reg-default-on-synthesis (the rejected Option A) is NOT performed — users who want synthesis-time defaults use `reg r[8] = K;` initializer syntax already routed through `seq.firreg`'s reset operand.
- **Stock CIRCT passes inside `-emit=hw`**: **Decision (specify-time clarification, 2026-05-04)**: `nslc -emit=hw` halts strictly at the nsl→CIRCT conversion boundary (Option A — strict boundary). The output is a mixed-dialect `hw`/`comb`/`seq`/`fsm`/`sv` ModuleOp. The four stock CIRCT passes (`convertFSMToSeq`, `lowerSeqToSV`, `prepareForEmission`, `exportVerilog`) are M7's responsibility. The README's "round-trip through stock CIRCT passes" gate (US5) runs them *externally* via `circt-opt`. This matches M5's `-emit=mlir` pattern (halt strictly post-pipeline), keeps M6 fixtures stable across CIRCT-version bumps, and decouples M6 from M7's emission stack.
- **Conversion failure on M5-clean input**: M5's `runNSLPasses` is the upstream gate; a structurally invalid IR reaching M6 would be an M5-pass bug. M6's expected behaviour: fail-fast with one `basic::DiagnosticEngine` diagnostic naming the `nsl::*` op that no pattern matched, plus `mlir::Location`. The conversion does NOT crash and does NOT silently leave `unrealized_conversion_cast` in the output.
- **Empty module**: An `nsl.module` with no body content (no declarations, no transfers) lowers to an empty `hw.module` body. The conversion does not insert synthetic ops to "make it valid" — empty is well-formed.
- **Module with only sim-only content** (`_display` calls, no synthesizable logic): the resulting `hw.module` body contains only `sv.ifdef "SIMULATION"`-guarded ops. Synthesis tools downstream are expected to elide the ifdef — that is M7's concern, not M6's.
- **Symbol collisions across modules**: M5 guarantees each `nsl.module` is its own scope; M6 preserves this (`hw.module @M_a` and `hw.module @M_b` are sibling top-level ops). No flat-namespace mixing.
- **Lowering pattern with multiple `nsl::*` operands of differing widths**: per design lines 1227–1245, the lowering is a 1:1 mapping to `comb.*`; `comb.*` ops require operand-width agreement, which S14/S15 + the M3 type system already enforce upstream. Any width mismatch reaching M6 is an M3 bug; M6 fail-fast with a diagnostic citing the source op.

## Requirements *(mandatory)*

### Functional Requirements

#### Library and pass surface

- **FR-001**: The `nsl-lower` library MUST expose a single new public-surface entry point — the `NSLToCIRCTPass` — re-exported from `include/nsl/Lower/Lower.h`. No new public header is created.
- **FR-002**: The `NSLToCIRCTPass` MUST register under the pass-name `-nsl-to-circt` so that `nsl-opt -nsl-to-circt` exercises it standalone on `.mlir` fixtures.
- **FR-003**: The `NSLToCIRCTPass` MUST use MLIR's `DialectConversion` framework (full conversion mode), with a `TypeConverter` mapping `!nsl.bits<W>` → `iW`, `!nsl.struct<@T>` → packed `iN` (where N is sum of field widths per S18 MSB-first packing), and other `!nsl.*` types per design §10's implicit shape.
- **FR-004**: The conversion-target legal-op set MUST be exactly: every op of `hw`, `comb`, `seq`, `fsm`, `sv` (Q1 → A: `hwarith` is NOT in the legal-op set). The illegal-op set MUST be every op of the `nsl` dialect.
- **FR-005**: Every conversion pattern MUST inherit the source `nsl::*` op's `mlir::Location` onto every CIRCT op it creates (`FusedLoc` when one source op produces multiple ops). No new MLIR-built-in `UnknownLoc` is introduced (Constitution Principle IV).

#### Per-op coverage (design §10 mapping table)

- **FR-006**: For every row of design §10's mapping table (lines 1206–1258), one corresponding lowering MUST exist in `NSLToCIRCTPass`. **PR #14 review-#16 fix (2026-05-05)**: the original wording specified "`mlir::ConversionPattern` subclass per row, registered in the pattern set" — the actual M6 implementation (Phase 4–6) places the lowering bodies INLINE inside two structural pre-pass functions (`lowerNSLModulesToHWModules` in `lib/Lower/Pass/CIRCTPatterns/ModulePatterns.cpp` and `lowerNSLProcsToFSMMachines` in `lib/Lower/Pass/CIRCTPatterns/FSMPatterns.cpp`) invoked from `NSLToCIRCTPass::runOnOperation` BEFORE `applyFullConversion` runs. Family-file `populate*Patterns` bodies are intentionally empty. Rationale + per-helper file-path map: `data-model.md` §3 architectural-deviation note. The bijection invariant (every design-§10 row has a lowering AND a fixture) is preserved; only the *grain* of "lowering" shifted from `OpConversionPattern<T>` records to inline helpers (`lowerArithOp`, `lowerBitOp`, etc.). Coverage guard refined per review-#17 fix (`coverage_guard.cmake`) to enforce per-family-directory fixture presence rather than per-`OpConversionPattern<` text grep.
- **FR-007**: `nsl.module` MUST lower to `hw.HWModuleOp` whose port list is derived from the paired `nsl.declare` block per design line 1208.
- **FR-008**: `nsl.reg` MUST lower to `seq.firreg` by default with **async, active-HIGH reset** (Q2 → C, PR #14 Round-1 review correction; previously specified active-LOW). The implicit reset port is `p_reset` (NSL-reserved per `nsl_lang.ebnf` §15 line 820); the FirRegOp's reset operand is the raw `%p_reset` block-arg with no `comb.icmp` adapter. The reset value is the `nsl.reg` initializer (`reg r[8] = K;` → reset value `K`; bare `reg r[8];` → reset value zero per S2/S23). When the enclosing module declares the `interface` modifier (S20), `nsl.reg` MUST lower to `seq.compreg` with explicit clock/reset operands derived from the user's declared interface names + polarities (design line 1209).
- **FR-009**: `nsl.wire` MUST lower to `hw.wire` (design line 1210).
- **FR-010**: `nsl.mem` MUST lower to `seq.firmem` with depth and width preserved (design line 1211).
- **FR-011**: `nsl.submodule` MUST lower to `hw.instance` referring to the target `hw.module` by symbol; in array form the M5 `NSLExplodeSubmodArrayPass` has already exploded it (design line 1257), so M6 sees only singleton submodules.
- **FR-012**: `nsl.transfer` (combinational) MUST lower to direct value substitution via `comb` ops; `nsl.clocked_transfer` MUST lower to a `seq.firreg` write (design lines 1213–1214).
- **FR-013**: `nsl.alt` MUST lower to a priority-encoded nested `comb.mux` chain (S13 priority semantics, design line 1215). `nsl.any` MUST lower to a per-target `comb.or` of all matching cases (S13 parallel semantics, design line 1216).
- **FR-014**: `nsl.if` (statement) MUST lower to `comb.mux` for wire LHS. For reg LHS it MUST lower via the **mux-on-data** strategy (Q3 → A): the `seq.firreg` from FR-008 receives `comb.mux(%cond, %new, %prev)` as its data input, where `%prev` is the reg's current SSA value. Chained `nsl.if`s nest the mux. The reg updates every cycle; conditional logic is in the data path. `seq.compreg` enable-port wiring is reserved for the explicit-`interface`-modifier path (FR-008). `seq.clock_gate` is not introduced at M6.
- **FR-015**: `nsl.proc` with `nsl.state` children MUST lower to `fsm.machine` with one `fsm.state` per `nsl.state` (design line 1218). The `nsl.first_state` reference MUST become the `fsm.machine`'s `initial_state` attribute (design line 1222). `nsl.goto` (state form, S25) MUST lower to `fsm.transition` (design line 1220).
- **FR-016**: `nsl.seq` inside an `nsl.func` MUST lower to `fsm.machine` with auto-generated states named `seq_N` (design line 1219); `nsl.goto` (label form, S25) inside an `nsl.seq` MUST lower to `fsm.transition` to the corresponding `seq_N` (design line 1221).
- **FR-017**: `nsl.finish` and `nsl.finish_method` MUST lower to `fsm.transition` to a synthetic sink state (design line 1223).
- **FR-018**: `nsl.call` to a `func_in` MUST lower to a direct combinational path plus a 1-bit valid signal (design line 1224); `nsl.call` to a `proc_name` MUST lower to `fsm.transition` to the target proc's initial state (design line 1225).
- **FR-019**: `nsl.sim_display`, `nsl.sim_finish`, `nsl.sim_init`, `nsl.sim_delay` MUST lower to `sv.fwrite`, `sv.finish`, `sv.initial`-equivalent, `sv.delay`-equivalent ops respectively, EACH wrapped in an `sv.ifdef "SIMULATION"` guard (design line 1226). The S29 `_init` block (module-level initialization, distinct from `nsl.sim_init`) MUST lower to a single `sv.initial { … }` op containing the conversion of every statement in the block, the whole `sv.initial` wrapped in `sv.ifdef "SIMULATION"`. Synthesis-time register defaults remain the territory of `reg r[8] = K;` initializer syntax (which routes through `seq.firreg`'s reset operand at FR-008); `_init` is sim-only.
- **FR-020**: All arithmetic / bit-op / comparison / reduction / extension / slice / concat / mux ops named in design lines 1227–1254 MUST lower to their named `comb.*` equivalent (Q1 → A: `comb`-only strategy). `nsl.sign_extend` lowers to `comb.concat (replicate MSB, operand)`; `nsl.zero_extend` lowers to `comb.concat (zeros, operand)`; signed comparisons use `comb.icmp slt`/`sle`/`sgt`/`sge`, unsigned use `comb.icmp ult`/`ule`/`ugt`/`uge`. No conversion pattern targets `hwarith.*` at M6.
- **FR-021**: `nsl.param_int` and `nsl.param_str` MUST lower to `hw.instance` parameter wires on every consuming `hw.instance` per S16 + design lines 1255–1256.
- **FR-022**: `nsl.structural_generate` MUST be absent from M6's input (eliminated by M5's `NSLExpandGeneratePass` per design line 1258); if present, M6 fail-fasts with a diagnostic naming this as an M5-pass bug.

#### Driver flag

- **FR-023**: The driver MUST accept `nslc -emit=hw input.nsl [-o output.mlir]`. With no `-o`, output goes to stdout.
- **FR-024**: `nslc -emit=hw` MUST invoke (in order) the M1 preprocessor, the M2 lexer + parser, the M3 sema, the M4 dialect lowering (M5's `lowerToNSL`), the M5 structural-expansion passes (`runNSLPasses`), and finally the M6 nsl→CIRCT conversion (`lowerToCIRCT`). On any stage's diagnostic-failure, the driver exits non-zero with the diagnostics already routed through `basic::DiagnosticEngine`.
- **FR-025**: `nslc -emit=hw` MUST print the resulting `mlir::ModuleOp` using MLIR's default printer with `mlir::OpPrintingFlags()` (matches M5's `-emit=mlir` convention; same flags for byte-stable output).
- **FR-026**: `nslc -emit=hw -` (read from stdin) is **DEFERRED at M6** (PR #14 review-fix). `tools/nslc/main.cpp` does not yet recognise `-` as a stdin marker for `-emit=hw`; the corresponding test fixture `test/Lower/circt/round_trip/stdin_pipe.test` (T139) is XFAIL'd with a deferral note. Full stdin support matches the convention used by `-emit=tokens`/`-emit=ast`/`-emit=mlir` from M3/M5; the gap is a small driver-side patch tracked in `tasks.md` "Post-implementation triage" deferred-work catalogue. Original wording mandated stdin support unconditionally — replaced with this deferral note for FR↔implementation alignment per `driver-emit-hw.contract.md` §5.
- **FR-027**: `nslc -emit=hw` MUST halt strictly at the nsl→CIRCT conversion boundary. It MUST NOT invoke any of the stock CIRCT passes (`convertFSMToSeq`, `lowerSeqToSV`, `prepareForEmission`, `exportVerilog`) — those are M7's responsibility. The output IR contains ops in exactly five dialects: `hw`, `comb`, `seq`, `fsm`, `sv` (Q1 → A: no `hwarith`). The US5 round-trip gate runs the stock passes externally via `circt-opt`.

#### Diagnostics and determinism

- **FR-028**: A conversion-pattern failure (no pattern matched a reachable `nsl::*` op) MUST emit one `basic::DiagnosticEngine` diagnostic naming the unmatched op's name and source location, then exit non-zero. The driver does NOT silently leave `unrealized_conversion_cast` in the output.
- **FR-029**: All output emitted by `nslc -emit=hw` MUST be byte-stable across two consecutive runs (Constitution Principle V — determinism). No process-id, ASLR, time, or pointer-value content leaks into the printer output.
- **FR-030**: The `mlir::Location` plumbing through the conversion MUST resolve back to source `.nsl` positions for every CIRCT op. The lit fixture `test/Lower/circt/loc_plumbing.nsl` MUST FileCheck-assert this on a representative input.

#### Build, dependencies, and test surface

- **FR-031**: The `lib/Lower/CMakeLists.txt` `add_nsl_library` call MUST add `CIRCTFSM` to `LINK_LIBS` (Q1 → A: no `CIRCTHwArith` is added). Other CIRCT link deps remain as M5 left them. No host-system `find_package` for CIRCT — the project's vendored CIRCT/MLIR continues to satisfy the link.
- **FR-032**: Test fixtures MUST be organized under `test/Lower/circt/<family>/` where families are: `module/` (US2), `fsm/` (US3), `arith/`, `state/`, `control/`, `sim/` (US4 sub-families), `round_trip/` (US5). One `<op>.nsl` + one `<op>.mlir.expected` golden per design §10 row, plus axis fixtures per US.
- **FR-033**: A CI guard (`test/Lower/circt/coverage_guard.cmake`) MUST enforce per-family-directory fixture presence: every family in the 9-row family→fixture-directory map (frozen by data-model.md §3) MUST contain at least one `*.nsl` or `*.mlir` fixture under its mapped `test/Lower/circt/<dir>/`. Adding a new family OR removing a family's last fixture MUST fail CI (Principle VI test discipline). **PR #14 review-#17 fix (2026-05-05)**: original wording specified "mechanically enumerate registered conversion patterns" via `OpConversionPattern<` text grep on family files — that approach was rendered ineffective by the inline-lowering deviation (every family file has an empty `populate*Patterns` body, so the grep reports zero hits and the bijection trivially passes even when a family's lowering is missing). The directory-presence rule is stricter: every family is unconditionally required to have a fixture. Future hardening (post-PR-#14): a per-family `lower<X>Op` helper-function regex over `ModulePatterns.cpp` + `FSMPatterns.cpp` to assert the inline-lowering helper exists. Naming convention is documented in each family-file header.
- **FR-034**: The M5 dialect surface (`dialect-api.contract.md` §2 freeze list) MUST stay byte-stable through M6 — adding a new `nsl::*` op means amending the M4 contract, not stealth-introducing it under the M6 banner. The M5 pass-pipeline contract similarly stays byte-stable.

### Key Entities *(include if feature involves data)*

- **`nsl::lower::NSLToCIRCTPass`**: the new M6 conversion pass. `mlir::OperationPass<mlir::ModuleOp>`. Owns the `TypeConverter` instance and the registered pattern set. Re-exported from `Lower.h`.
- **`nsl::lower::CIRCTTypeConverter`**: maps `!nsl.bits<W>` → `iW`, `!nsl.struct<@T>` → packed `iN` per S18 MSB-first ordering. Internal to `nsl-lower`.
- **Conversion patterns (one per design §10 row)**: `mlir::ConversionPattern` subclasses, each rewriting one `nsl::*` op kind to its CIRCT counterpart. Registered at pass-construction time. Internal.
- **`Compilation::lowerToCIRCT`**: new public member function on `nsl::driver::Compilation` (header `nsl/Driver/Compilation.h`). Returns `mlir::LogicalResult`. Routes conversion failures through `basic::DiagnosticEngine`.
- **`CompileOptions::EmitKind::HW`**: existing enum value (frozen at M3 per design §11 line 1281); M6 wires it from "stub" to "operational" by adding the `lowerToCIRCT` invocation in `Compilation::emit`.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: A contributor can run `nslc -emit=hw <any-M5-clean-input.nsl>` and observe a non-empty `.mlir` output containing zero ops in the `nsl` dialect within the same wall-clock budget as `nslc -emit=mlir` for the same input + 25% (the conversion is bounded; no quadratic blowup).
- **SC-002**: 100% of design §10's mapping-table rows (currently ~40 rows, lines 1206–1258) have a registered conversion pattern AND a matching FileCheck fixture under `test/Lower/circt/`. The CI coverage guard reports zero gaps.
- **SC-003**: For every fixture in `test/Lower/circt/round_trip/`, two consecutive `nslc -emit=hw` invocations produce byte-identical output (`diff` returns empty); `circt-opt --convert-fsm-to-sv --lower-seq-to-sv` (corrected recipe per review-#15 fix) over the output exits zero with no diagnostics.
- **SC-004**: For every fixture, every CIRCT op in the output carries an `mlir::Location` resolvable to a `.nsl:line:col` (no `UnknownLoc`); a lit grep guard asserts this.
- **SC-005**: Adding a new `nsl::*` op to the M4 dialect (post-amendment) requires (a) one new conversion pattern in `NSLToCIRCTPass`, (b) one new fixture pair under `test/Lower/circt/`, (c) one row added to design §10's mapping table — and the CI coverage guard enforces (a)+(b) consistency. No silent additions.
- **SC-006**: A contributor running `nsl-opt -nsl-to-circt input.mlir` standalone on a hand-authored `.mlir` fixture (no `nslc` driver involved) produces the same output as `nslc -emit=hw <equivalent>.nsl` for the same input shape — the pass is well-isolated and reusable.
- **SC-007**: The `nsl-lower` library link surface adds exactly one new CIRCT library at M6: `CIRCTFSM` (Q1 → A: no `CIRCTHwArith`); `CIRCTSV` is added if and only if M5 did not already link it. No host-system CIRCT dependency. Vendored CIRCT continues to satisfy the link.
- **SC-008**: Constitutional anchors are honored: VI (every conversion pattern has a lit + FileCheck test), III (no hand-rolled CIRCT-equivalent passes — the lowering produces real `circt::*` ops, not lookalikes), V (determinism gate per SC-003), IV (diagnostic plumbing crosses the seam — SC-004 enforces).

## Assumptions

- The M5 `nsl-lower` library and `Lower.h` umbrella header are landed and stable. M6 extends them; M6 does not redesign M5's interface.
- The M4 `nsl` dialect ABI (`dialect-api.contract.md` §2 freeze list) is stable through M6. New `nsl::*` ops cannot land under the M6 banner — they require an M4-contract amendment first (matches the M5 spec's identical clause).
- The vendored CIRCT/MLIR build (per the dev-container `ghcr.io/koyamanx/nsl-nslc:dev` setup) provides `circt::hw`, `circt::comb`, `circt::seq`, `circt::fsm`, `circt::sv` libraries linkable via `add_nsl_library`'s `LINK_LIBS` field. (`circt::hwarith` is also available in the vendored snapshot but is intentionally unused at M6 per Q1 → A.) No host-system CIRCT install is required.
- The audited NSL projects (`cpu16`, `mips32_single_cycle`, etc.) are NOT required to compile end-to-end at M6 — that is M7's `P-VEN`/`P-VCD` deliverable. M6's fixtures are hand-authored representative samples.
- M6 inherits M5's determinism guarantee (printer flags, file-list permutation invariance, pointer-value scrubbing). M6 introduces no new sources of non-determinism.
- The seven `_`-prefixed simulation system tasks per `S17`/§10 (`_display`, `_finish`, `_init`, `_delay`, plus any that landed at M3/M4) all have CIRCT equivalents under `sv` (`sv.fwrite`, `sv.finish`, `sv.initial`, `sv.delay`). If `circt::sv` lacks a target for any specific NSL system task, the spec is amended — not silently dropped.
- The `_init` block (S29) lowering is sim-only (`sv.initial` under `sv.ifdef "SIMULATION"`) — resolved at specify-time, see Edge Cases / FR-019. Synthesis-time register defaults remain the territory of `reg r[8] = K;` initializer syntax routed through `seq.firreg` reset operands.
- `nslc -emit=hw` halts strictly at the nsl→CIRCT conversion boundary — resolved at specify-time, see Edge Cases / FR-027. The four stock CIRCT passes (`convertFSMToSeq`, `lowerSeqToSV`, `prepareForEmission`, `exportVerilog`) are M7's responsibility.
- The arithmetic-dialect strategy is `comb`-only (Q1 → A in `## Clarifications` 2026-05-04). `hwarith` adoption is deferred to a future post-M6 refactor; M6 fixtures and `LINK_LIBS` reflect this choice end-to-end.
