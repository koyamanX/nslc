<!-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception -->

# Research: M6 — `nsl-lower` part 2 (`nsl` → CIRCT lowering)

**Branch**: `010-m6-circt-lowering` | **Date**: 2026-05-04
**Plan**: [plan.md](./plan.md)

This file resolves every Technical Context decision with a
**Decision / Rationale / Alternatives considered** entry,
mirroring the pattern established in
[`specs/008-m5-structural-passes/research.md`](../008-m5-structural-passes/research.md).
Each decision is anchored in the Constitution, the design docs, the
spec FRs, the three Clarifications-resolved questions (Q1–Q3 in
[`spec.md`](./spec.md) §Clarifications), prior-milestone precedent
(M0–M5), or upstream MLIR / CIRCT conventions — no decision is made
on "engineering taste" alone.

---

## 1. Conversion infrastructure: full-conversion `DialectConversion`

**Decision**: `NSLToCIRCTPass` is implemented as an
`mlir::OperationPass<mlir::ModuleOp>` that constructs a
`mlir::ConversionTarget`, registers an
`nsl::lower::CIRCTTypeConverter`, populates an
`mlir::RewritePatternSet` with one `mlir::OpConversionPattern<T>`
per design-§10 mapping-table row, and invokes
`mlir::applyFullConversion(module, target, std::move(patterns))`.
The conversion target marks the entire `nsl` dialect as illegal
(`target.addIllegalDialect<nsl::dialect::NSLDialect>()`) and the
five CIRCT dialects as legal
(`target.addLegalDialect<circt::hw::HWDialect, circt::comb::CombDialect, circt::seq::SeqDialect, circt::fsm::FSMDialect, circt::sv::SVDialect>()`).
On any unmatched `nsl::*` op, `applyFullConversion` returns
`mlir::failure()`; the pass routes that failure through the
M5-introduced `DiagnosticBridge` to surface a single
`basic::DiagnosticEngine` diagnostic of the form
`error: nsl→CIRCT conversion failed for op '<dialect.opname>'` at
the offending op's `mlir::Location`.

**Rationale**: Full-conversion mode is the canonical MLIR idiom
when "every op of dialect X must become an op of dialect Y∪Z" —
which is exactly M6's mandate per FR-004. Using
`mlir::applyPartialConversion` would permit residual
`unrealized_conversion_cast` ops to leak into `-emit=hw` output,
which FR-028 explicitly prohibits. `OpConversionPattern<T>`
(rather than the generic `RewritePattern`) gives each pattern a
typed `mlir::OpAdaptor` view of operand types and operand counts,
catching M5-bug shapes at compile time. The
`applyFullConversion` failure path naturally surfaces *which* op
was unmatched via the standard MLIR diagnostic chain — the
`DiagnosticBridge` from M5 captures it without rework.

**Alternatives considered**:

- *Partial conversion*: rejected — FR-028 forbids
  `unrealized_conversion_cast` in output; partial mode is for
  multi-stage lowerings, not the M6 single-shot.
- *Hand-rolled walker (no `DialectConversion`)*: rejected — would
  require re-implementing type conversion, operand legalisation,
  and use-replacement bookkeeping that MLIR upstream provides for
  free. Constitution Principle III's "no hand-rolled CIRCT-
  equivalent passes" principle applies above the dialect (CIRCT's
  passes); below the dialect (MLIR's machinery) there is no such
  prohibition, but reinventing is still gratuitous.
- *Two-stage conversion (nsl → "nsl-canonical" → CIRCT)*:
  rejected — design §10 names a single pass `NSLToCIRCTPass`; an
  intermediate `nsl-canonical` form would be a new dialect with
  its own freeze contract. M5's six-pass pipeline already
  canonicalises `nsl::*` IR before M6 sees it; no further
  canonicalisation is needed.

---

## 2. Type conversion: `CIRCTTypeConverter` minimal form

**Decision**: `CIRCTTypeConverter` extends `mlir::TypeConverter`
and registers two type-conversion lambdas:

```cpp
addConversion([](nsl::dialect::BitsType t) -> mlir::Type {
    return mlir::IntegerType::get(t.getContext(), t.getWidth());
});

addConversion([this](nsl::dialect::StructType t) -> mlir::Type {
    // Lookup struct decl in M5's nsl.module symbol table; sum
    // field widths per S18 MSB-first packing; return iN.
    auto totalWidth = computeStructPackedWidth(t);
    return mlir::IntegerType::get(t.getContext(), totalWidth);
});
```

Plus a value-materialisation lambda for `BitsType` → `IntegerType`
that wraps the source SSA value in a `comb.bitcast` (a no-op when
both ops are already `iN`-shaped, but legal even when the source is
a `BlockArgument` that hasn't been rewritten yet). No materialisation
for `StructType` — struct-typed SSA values are decomposed by
M5's `NSLExpandVariablesPass` (struct-SSA-split) before M6 sees
them; any surviving `!nsl.struct` value is an M5-pass bug, fail-
fast per FR-022 / FR-028.

**Rationale**: M5's IR contains exactly two `!nsl.*` types reachable
from any `nsl::*` op operand or result: `!nsl.bits<W>` (general bit-
vector) and `!nsl.struct<@T>` (named struct, already reduced to
per-field `!nsl.bits` by struct-SSA-split). The control-terminal
"types" (`clk_terminal`, `reset_terminal`, function-arg valid
signals) are NOT distinct MLIR types in the M4 dialect — they are
`!nsl.bits<1>` with a directional attribute. So the TypeConverter
needs only the two cases. Width arithmetic for `!nsl.struct<@T>`
follows S18 MSB-first packing (sum of field widths, fields in
declaration order with the first field at the MSB end) — the M3
type-system already exposed this width via `TypeSystem::sizeOf(...)`
which the converter calls.

**Alternatives considered**:

- *Per-field `!nsl.struct` lowering with `comb.concat` insertion in
  the converter*: rejected — duplicates M5's struct-SSA-split work;
  the M5 contract states post-pipeline IR has zero `!nsl.struct`
  values (only `!nsl.struct` types on field-access ops which are
  rewritten by struct-SSA-split). Trusting M5's contract avoids
  the duplicate path.
- *Mapping `!nsl.bits<1>` to `i1` and `!nsl.bits<N>` to `iN`
  separately*: rejected as gratuitous specialisation —
  `mlir::IntegerType::get(ctx, 1)` and `IntegerType::get(ctx, N)`
  produce the same `IntegerType` family; one lambda handles both.
- *Mapping `!nsl.bits<0>` (zero-width)*: M3 Sema rejects zero-width
  bit declarations (`S2`, `S15`); zero-width values cannot reach
  M6. The converter does not handle them — an assert covers the
  invariant violation if M5 produces one.

---

## 3. FSM lowering target: `circt::fsm::FSMDialect` (`fsm::MachineOp`)

**Decision**: `nsl.proc` with its `nsl.state` children lowers to a
single `circt::fsm::MachineOp`. The `MachineOp`'s `initial_state`
attribute is set from the `nsl.first_state` symbol-ref. Each
`nsl::StateOp` becomes a child `circt::fsm::StateOp` whose body
region holds the conversion of the `nsl::StateOp`'s body. Each
`nsl::GotoOp` (state form per S25) becomes a `circt::fsm::
TransitionOp` linking the source state to the target state.
`nsl::SeqOp` inside an `nsl::FuncOp` lowers to a `MachineOp` with
auto-generated state names `seq_0`, `seq_1`, … (design §10 line
1219); each label-form `nsl::GotoOp` inside the seq becomes a
`fsm::TransitionOp` to the matching `seq_N`. `nsl::FinishOp` and
`nsl::FinishMethodOp` materialise as transitions to a synthetic
sink state named `__sink__` per `MachineOp`. `nsl::CallOp` to a
`proc_name` becomes an `fsm::TransitionOp` whose target is the
called proc's initial state; `nsl::CallOp` to a `func_in` is
NOT a state transition — it stays as a combinational invocation
(see §5).

**Rationale**: The README §Roadmap M6 row literal text names this:
"`nsl::ProcOp` / `nsl::StateOp` / `nsl::SeqOp` lower to
`fsm::MachineOp`". CIRCT's `fsm` dialect is purpose-built for
this lowering pattern; it has the right shape (`MachineOp` →
`StateOp` → `TransitionOp`), exactly matching `nsl::*`'s
proc/state/goto shape. The seq-inside-func auto-naming convention
follows design §10 line 1219 verbatim. The synthetic `__sink__`
state for finish-ops is the canonical CIRCT idiom for terminal
transitions (visible in CIRCT's own fsm tests). This is the
decision Constitution Principle III names: "Hand-rolled netlist,
RTL, register-inference, or state-machine-lowering passes are NOT
permitted; if a needed primitive is missing, the work belongs
upstream in CIRCT". Since `fsm` exists and works, M6 uses it.

**Alternatives considered**:

- *Flatten to encoded-state `seq.firreg` + comb mux directly*:
  rejected — that is exactly the hand-rolled state-machine
  lowering Principle III prohibits. The stock CIRCT pass
  `circt::fsm::convertFSMToSeq` does this flattening at M7 time,
  on M6's output.
- *Multiple `MachineOp`s per `nsl.proc` (one per state group)*:
  rejected — NSL semantics treats a `proc` as a single state
  machine with one current-state register; producing multiple
  `MachineOp`s would require an M7 re-merge step that adds no
  value at M6.
- *Use `MachineOp`'s nascent calyx-style hierarchical machines*:
  rejected — CIRCT's `fsm` dialect at the vendored snapshot does
  not stably support hierarchical machines; M6 stays on the
  flat-machine path.

---

## 4. Reset polarity / synchronicity for default `seq.firreg`

**Decision**: Per spec Q2 → Option C. When `nsl.reg` lowers via
the no-`interface`-modifier path (FR-008), the resulting
`circt::seq::FirRegOp` is constructed with:

- **Async reset**: `seq::FirRegOp::build(...)` with a non-null
  `reset` operand connected to the implicit module-level `rst_n`
  signal.
- **Active-low polarity**: encoded as the `is_async` attribute set
  to `true` and the reset condition computed as `comb.icmp eq
  %rst_n, 0` (i.e., reset fires when `rst_n` is low). The
  `seq.firreg`'s reset_value operand carries the `nsl.reg`
  initializer (`reg r[8] = K;` → reset value `K`; bare
  `reg r[8];` → reset value zero per S2/S23).

The implicit module-level `rst_n` port is materialised by the
`nsl.module` → `hw.HWModuleOp` lowering (US2 / FR-007) when no
S20 `interface` modifier is present: a single 1-bit input port
named `rst_n` is added at the end of the input port list. Same
treatment for the implicit `clk` port. Modules using non-standard
port names (`CLK`, `RESET`, `nreset`, …) must use the explicit
S20 `interface` modifier; M6 does not auto-rename.

**Rationale**: Spec Q2 → C is settled at the spec level; this
section documents the *implementation* of that decision.
Async-active-low matches the audited NSL corpus's reference
Verilog patterns (`always @(negedge rst_n …)`). The
`comb.icmp eq %rst_n, 0` formulation produces a 1-bit
"reset-fires" condition that `seq::FirRegOp` consumes naturally;
the alternative of toggling FirRegOp's "polarity" attribute
directly is rejected because the vendored CIRCT snapshot exposes
only async/sync polarity, not active-high/active-low — the
high/low aspect lives entirely in the upstream condition wire.

**Alternatives considered**:

- *Encode active-low directly via a CIRCT FirRegOp polarity
  attribute*: not feasible at the vendored CIRCT version; would
  require a CIRCT amendment.
- *Synthesize an inverter once at the module port and reuse the
  inverted signal for every reg's reset operand*: this is
  semantically equivalent and produces strictly less IR output
  (one inverter shared vs N inverters). M6 elects the
  per-reg-`comb.icmp` form for simpler lit-fixture goldens
  (every fixture's IR is locally complete; the shared-inverter
  form requires reasoning about a module-level wire); CIRCT's
  `prepareForEmission` pass at M7 will common-subexpression-
  eliminate the redundant inverters anyway.
- *Configurable per-CLI-flag*: rejected — the spec Q2 → C
  established a project-wide convention; per-flag config would
  fragment the audited-corpus golden VCDs (M7).

---

## 5. Conditional reg-update: mux-on-data (Q3 → Option A)

**Decision**: `nsl.if` over a `reg` LHS lowers via the mux-on-data
strategy. The `circt::seq::FirRegOp` for the affected `nsl.reg`
receives `comb.mux(%cond, %new_value, %prev_state)` as its data
input, where `%prev_state` is obtained via a `seq.read`
(reading the FirRegOp's current value). Chained `nsl.if`s nest
the `comb.mux` (innermost = innermost branch). One `seq.firreg`
per `nsl.reg` regardless of how many `nsl.if`s the reg
participates in. `seq.compreg` is reserved exclusively for the
explicit-`interface`-modifier (S20) path (FR-008 unchanged); the
clock-gating (`seq::ClockGate`) op is NOT introduced at M6.

The `nsl.if` over a `wire` LHS lowers to `comb.mux` directly in
the data path (no reg involvement); this is the conventional case
and matches design line 1217's "for wire LHS" half.

**Rationale**: Spec Q3 → A is settled; this section documents the
implementation. Chained `nsl.if` semantics are right-to-left in
NSL (innermost branch wins) — `comb.mux` nesting naturally encodes
this: `comb.mux(%outer_cond, comb.mux(%inner_cond, %inner_new,
%prev), %fallback)`. Reading the previous register state via
`seq.read` is the standard CIRCT idiom; the read is implicitly
available in the same module body (no explicit op emitted, since
`seq.firreg`'s result is the read view).

**Alternatives considered**:

- *Switch to `seq.compreg` when conditional present*: rejected at
  spec time (Q3 Option B) — see spec §Clarifications.
- *Use `seq::ClockGate`*: rejected at spec time (Q3 Option C) —
  see spec §Clarifications.
- *Synthesize a `comb.if` op (CIRCT does not have one)*: not
  applicable — `comb` dialect has no statement-level `if`; only
  `comb.mux` for combinational selection.

---

## 6. `nsl.alt` priority encoding via nested `comb.mux`

**Decision**: `nsl.alt { case A: x; case B: y; … case Z: z; default: d; }`
lowers to a right-associative chain of `comb.mux` ops:

```
comb.mux(%caseA_cond, %caseA_value,
  comb.mux(%caseB_cond, %caseB_value,
    …
      comb.mux(%caseZ_cond, %caseZ_value, %default_value)))
```

Source-order is priority-order (S13 priority semantics): the
first matching case wins. The conversion pattern visits the cases
in source order and folds them right-to-left. If no `default` is
present, the fallback value is the LHS's prior state (for reg LHS,
the `seq.read` of the FirRegOp; for wire LHS, an `hw.constant 0`
of the LHS width — matching NSL's "unset wire" semantics).

**Rationale**: `comb.mux` is the canonical CIRCT primitive for
priority-encoded selection; the right-associative nest is the
direct encoding of S13 priority semantics. The conversion is
mechanical (one pattern per `nsl.alt` case shape; no per-case
specialisation needed).

**Alternatives considered**:

- *`comb.array_create` + `comb.array_get` (one-hot encoded
  priority)*: rejected — produces wider IR and forces
  `prepareForEmission` to flatten anyway; the nested-mux form is
  simpler and what every CIRCT codebase uses.
- *Materialise a synthetic `nsl.alt` → `comb.case` op (CIRCT does
  not have this)*: not applicable.

---

## 7. `nsl.any` parallel-OR encoding via per-target `comb.or`

**Decision**: `nsl.any { case A: x; case B: y; }` (S13 parallel
semantics: every matching case fires; per-target the values
union via OR) lowers to per-target `comb.or` reduction. For each
target wire/reg LHS in the cases, the conversion gathers the
case-conditional values and builds:

```
comb.or(
  comb.mux(%caseA_cond, %caseA_value, 0),
  comb.mux(%caseB_cond, %caseB_value, 0),
  …)
```

per-target. This produces the OR of all matching contributions,
where non-matching cases contribute zero.

**Rationale**: Design §10 line 1216 names this: "`nsl.any` →
per-target `comb.or` of all matching cases". The
`comb.mux(%cond, %value, 0)` envelope is the standard CIRCT idiom
for "value if condition else zero-extended-width-zero".

**Alternatives considered**:

- *`comb.parity`*: applicable only to 1-bit reductions, not
  general-width OR reduction.
- *Hand-fused `comb.or` with internal mask*: produces denser IR
  but obscures the per-case structure that lit-fixture goldens
  benefit from displaying. Rejected for fixture readability.

---

## 8. `nsl.call` to `func_in`: combinational handshake

**Decision**: `nsl.call @F(args)` where `@F` is a `func_in` lowers
to: (a) the function body inlined as a sub-region in the calling
module's body (preserving SSA values via the standard
`OpConversionPattern` rewriter mechanism), and (b) a 1-bit "valid"
signal materialised as an `hw.wire` named `<func_name>_valid` that
is high exactly when the call is structurally executed (i.e., the
enclosing-`nsl.if`-conditional ANDed together via `comb.and`). The
function's return value flows back through standard SSA; the
valid signal flows through the `hw.wire`. Cross-module func_in
calls — where the `func_in` lives in a sibling submodule — are
NOT supported at M6 (NSL's grammar permits `nsl.call` only to
same-module symbols per N6); cross-module calls go through
submodule-method invocation, which lowers via FR-018's proc-call
path.

**Rationale**: Design §10 line 1224 names this: "`nsl.call` to
`func_in` → direct combinational path + 1-bit valid signal". The
`hw.wire` form for the valid signal is the canonical CIRCT
representation of an internal-named net. The structural-conditional
AND is computable at lowering time because the M5 IR has a
well-defined enclosing-`nsl.if`-stack for every op (the
`nsl.if`'s `cond` operand is in scope at the call site). For
`func_self`, the same lowering applies (the function body is the
enclosing module's own body; the inline is structurally a no-op).

**Alternatives considered**:

- *Materialise the valid signal as an additional output port on
  the enclosing `hw.module`*: rejected — would force every call
  site to thread the valid back to a top-level port, breaking
  encapsulation.
- *Tuple-result on the function's return type*: rejected — would
  change the M3 type signature of every `func_in` and force an
  M3 amendment.
- *Skip the valid signal entirely*: rejected — design §10 line
  1224 explicitly names it; downstream tooling (LSP `inlayHint`
  at T10) is expected to surface it.

---

## 9. Sim-only ops: `sv.ifdef "SIMULATION"` guard wrapping

**Decision**: All `nsl.sim_display`, `nsl.sim_finish`,
`nsl.sim_init`, `nsl.sim_delay` ops, plus the M6-introduced
lowering of the S29 `_init` block, are wrapped in an
`sv::IfDefOp` whose `cond` attribute is `"SIMULATION"`. The
ifdef is materialised once per containing-`hw.module` body (not
once per sim-op) — all sim-only ops in a module share a single
ifdef block to keep the output IR compact. Inside the ifdef body:
- `nsl.sim_display` → `sv::FWriteOp` with the format string and
  args translated.
- `nsl.sim_finish` → `sv::FinishOp`.
- `nsl.sim_delay` → `sv::DelayOp`.
- `nsl.sim_init` (the per-sim-op variant; distinct from S29
  `_init`) → contributes statements inside an `sv::InitialOp`.
- S29 `_init` block → its body becomes a single `sv::InitialOp`
  inside the ifdef.

The conversion pattern infrastructure manages the per-module
ifdef as a "lazy create" — the first sim-only op encountered in a
module triggers ifdef-block creation; subsequent ops insert into
the existing block.

**Rationale**: Design §10 line 1226 names the `sv.ifdef
"SIMULATION"` guard. Sharing one ifdef per module (rather than
per op) keeps the output IR compact and matches the canonical
SystemVerilog idiom (synthesis tools elide the entire ifdef
block in one pass). The S29 `_init` decision is per spec
specify-time Q1 → B (sim-only via `sv.initial` under ifdef); the
implementation is a single `sv::InitialOp` containing the
converted body.

**Alternatives considered**:

- *Per-op ifdef wrapping*: rejected — produces N×2 lines of
  output IR per op; the per-module-shared form is strictly
  smaller and equivalent under elision.
- *Two ifdef blocks (one for `sim_*`, one for `_init`)*: rejected
  for the same reason; both go in the same SIMULATION ifdef.
- *Different macro name for `_init` (e.g., `INITIALIZATION`
  rather than `SIMULATION`)*: rejected — would force users to
  define two separate macros for sim-only behaviour, surprising.

---

## 10. Driver wiring: `Compilation::lowerToCIRCT` + `EmitKind::HW`

**Decision**: `Compilation::lowerToCIRCT(mlir::ModuleOp module)`
returns `mlir::LogicalResult`. Implementation:

```cpp
mlir::LogicalResult Compilation::lowerToCIRCT(mlir::ModuleOp m) {
    mlir::PassManager pm(mlirCtx_);
    pm.addPass(nsl::lower::createNSLToCIRCTPass());
    DiagnosticBridge bridge(mlirCtx_, diag_);
    return pm.run(m);
}
```

`Compilation::emit` is extended with an `EmitKind::HW` arm that
calls `lowerToNSL` → `runNSLPasses` → `lowerToCIRCT` → MLIR
default-printer (`module.print(os, mlir::OpPrintingFlags())`).
The `-emit=hw` driver flag is wired in `tools/nslc/main.cpp` per
the existing M3-frozen flag-parser shape; no new CLI parser code
is required (the enum value `EmitKind::HW` was reserved at M3 per
design §11 line 1281).

**Rationale**: Direct mirror of M5's `Compilation::lowerToNSL` /
`runNSLPasses` shape. The PassManager-based invocation is the
standard MLIR driver idiom — the same DiagnosticBridge RAII
handler M5 introduced is reused unchanged.

**Alternatives considered**:

- *Direct in-line conversion call* (no PassManager): rejected —
  loses the `nsl-opt -nsl-to-circt` standalone path (FR-002), and
  bypasses MLIR's pass timing/instrumentation if needed for
  performance regression diagnosis.
- *Multi-pass pipeline at `EmitKind::HW`*: rejected — Q2-specify-time
  → A established strict-boundary halt; only one pass runs.

---

## 11. Pattern registration: per-family `populate*Patterns` helpers

**Decision**: Each `lib/Lower/CIRCTPatterns/<family>Patterns.cpp`
file exposes a single `populate<Family>Patterns` function that
takes the `mlir::RewritePatternSet&` and `CIRCTTypeConverter&` by
reference and adds its family's patterns. `NSLToCIRCTPass` calls
these in a fixed order (alphabetic by family for determinism):

```cpp
populateArithPatterns(patterns, converter);
populateBitOpPatterns(patterns, converter);
populateControlPatterns(patterns, converter);
populateFSMPatterns(patterns, converter);
populateModulePatterns(patterns, converter);
populateParamPatterns(patterns, converter);
populatePortPatterns(patterns, converter);
populateSimPatterns(patterns, converter);
populateStatePatterns(patterns, converter);
```

This call order is byte-stable; pattern application order inside
`applyFullConversion` is determined by MLIR's pattern-benefit and
greedy-rewrite traversal, both of which are deterministic.

**Rationale**: This is the standard MLIR conversion-pass shape
used across CIRCT's own conversions (`HandshakeToHW`, `FSMToSeq`,
etc.) — one `populate<Family>Patterns` per file, one call site
per pass. Per-family file split keeps each file under ~250 LOC
and matches the human-cognitive grouping of design §10's
mapping table (arithmetic together, FSM together, etc.).

**Alternatives considered**:

- *Single monolithic `NSLPatterns.cpp`*: rejected — would be
  ~1500–2000 LOC, difficult to navigate, and merge conflicts on
  unrelated pattern adds spike.
- *One file per design-§10 row*: rejected — too granular
  (~40 files, each ~30 LOC); per-family is the right grain.

---

## 12. Pattern-benefit / tiebreak policy

**Decision**: All M6 patterns register with default benefit = 1.
No M6 pattern has a specialised fast-path that would need higher
benefit. If a future pattern needs specialisation (e.g., a
specialised `nsl.add` for power-of-two operands), it gains
benefit = 2 to win over the generic. This is a documented
forward-policy; M6 itself uses uniform benefit.

**Rationale**: Default benefit is the standard MLIR idiom; pattern-
ordering tiebreaks are deterministic under MLIR's greedy rewriter
without explicit intervention. Reserving higher benefits for
specialised future patterns keeps the door open for performance
work.

**Alternatives considered**:

- *Per-pattern benefit tuning*: rejected as premature; no M6
  pattern has a specialisation that needs it.

---

## 13. Determinism audit: extends M5's CI grep

**Decision**: The CI determinism audit M5 introduced
(forbids `std::unordered_*`, pointer-derived ordering,
`std::chrono::*` timestamps in `lib/Lower/`) extends to all M6
code paths (`lib/Lower/NSLToCIRCTPass.cpp`,
`lib/Lower/CIRCTTypeConverter.cpp`,
`lib/Lower/CIRCTPatterns/*.cpp`). No new audit rules are added at
M6 — the M5 ruleset is sufficient because M6's code is structurally
the same (visit-the-IR, build new ops). The two-host-path
`diff -q` determinism gate from M5 is extended to compare
`nslc -emit=hw` output across two builds — both `-emit=mlir` and
`-emit=hw` outputs must be byte-stable.

**Rationale**: Reuses the M5 mechanism unchanged; no new infra.

**Alternatives considered**:

- *New M6-specific audit rules*: not needed — M6 introduces no
  new sources of non-determinism.

---

## 14. Test corpus organization: one fixture per design-§10 row

**Decision**: For each row of design §10's mapping table (lines
1206–1258), one `<op>.nsl` + `<op>.mlir.expected` pair under the
appropriate `test/Lower/circt/<family>/` directory. Naming
follows lowercase-snake-case op name (`nsl.add` →
`add.nsl`/`add.mlir.expected`). The CI coverage guard
(`coverage_guard.cmake`) runs at configure time, walks
`registerCIRCTPatterns` in `NSLToCIRCTPass.cpp` to enumerate
registered patterns, walks `test/Lower/circt/` to enumerate
fixtures, and asserts a bijection. Adding a new pattern without
a fixture (or a fixture without a pattern) fails CI.

Plus axis fixtures per US: US2 module fixtures
(`test/Lower/circt/module/` — port-shape variants), US3 FSM
fixtures (`test/Lower/circt/fsm/` — single-state, two-state,
finish, seq-inside-func, cross-call), US4 sim fixtures
(`test/Lower/circt/sim/` — one per sim-only op + S29 `_init`),
US5 round-trip fixtures (`test/Lower/circt/round_trip/` — five
representative full-module fixtures asserting byte-identical
double-emission + circt-opt-clean round-trip).

**Rationale**: FR-032 + FR-033 verbatim. The bijection guard
implements TDD's "every pattern has a test" invariant
mechanically, so a contributor cannot accidentally land a pattern
without coverage.

**Alternatives considered**:

- *Combine multiple design-§10 rows into one fixture*: rejected —
  diagnostic granularity drops; CodeRabbit / lit failure messages
  no longer pinpoint which pattern broke.
- *No coverage guard, rely on review*: rejected — Principle VIII
  TDD is NON-NEGOTIABLE; the guard mechanises it.

---

## 15. Forward-looking M7 hooks (not landing at M6)

**Decision**: M6 lays NO M7 wiring. The four stock CIRCT passes
(`circt::fsm::convertFSMToSeq`, `circt::seq::lowerSeqToSV`,
`circt::sv::prepareForEmission`, `circt::exportVerilog`) are NOT
invoked from `nslc -emit=hw` (Q2-specify-time → A). The audited
corpus under `test/audited/` is NOT exercised at M6 (FR-032
hand-authored corpus only). `nsl-driver` (the layer-9 binary
that wraps `nslc` with project-build context) is M7. Formal
verification (`riscv-formal` for `rv32x_dev`) is M8. The
`-emit=verilog` flag stays stubbed at M6 — its enum value
`EmitKind::Verilog` was reserved at M3.

**Rationale**: Strict milestone-scope discipline per the
README §Roadmap row delineation. Cross-milestone scope creep is
explicitly forbidden by the Constitution Governance section
("Milestone plan" clause). M7's deliverables have their own spec
+ plan + tasks artefacts authored when M6 lands.

**Alternatives considered**:

- *Pre-stage M7's pass invocation behind a flag*: rejected — adds
  test-fixture surface for an unvetted pipeline shape; the M5
  spec rejected the equivalent move (post-conversion CIRCT-pass
  invocation in `-emit=mlir`) for the same reason.
