<!-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception -->

# Contract: `nsl::*` → CIRCT op-mapping (M6 freeze of design §10)

**Branch**: `010-m6-circt-lowering` | **Date**: 2026-05-04
**Spec**: [../spec.md](../spec.md) | **Plan**: [../plan.md](../plan.md)

This contract pins down — for the duration of M6 — the exact CIRCT
op produced by `NSLToCIRCTPass` for each `nsl::*` op kind. The
table is derived from
[`docs/design/nsl_compiler_design.md`](../../../docs/design/nsl_compiler_design.md)
§10 lines 1235–1305, **frozen by reference**: any change to
design §10's table is also a change to this contract. Adding,
removing, or re-targeting a row is a Principle VII coupling
change requiring spec + design + plan + this contract all updated
in one PR.

---

## §1. Frozen op-mapping table

| `nsl::*` op | CIRCT target | Pattern family | Notes |
|---|---|---|---|
| `nsl::ModuleOp` | `circt::hw::HWModuleOp` | ModulePatterns | Port list from paired `nsl::DeclareOp` body (§3); body-region conversion recurses on contents (incl. the in-module `nsl::InputPortOp` / `OutputPortOp` / `InoutPortOp` rewritten as block-arg substitutions / output wiring) |
| `nsl::DeclareOp` | (consumed during ModuleOp lowering) | ModulePatterns | Not a standalone target — `nsl::DeclareOp`'s `pair_name` matches a sibling `nsl::ModuleOp`'s `sym_name` (post-merge M4-amendment #9; sibling pairing because `sym_name` would collide). The declare body's port-info children are walked to build the `hw::HWModuleOp` port list |
| `nsl::InputPortOp` (declare-body placement) | (consumed during ModuleOp lowering) | ModulePatterns / PortPatterns | Materialises one input port on the resulting `hw::HWModuleOp`, named `name`, typed `i<W>`. M5 emits this op in TWO places (dual-placement per amendment-#9): the declare-body form is metadata for port-list derivation here |
| `nsl::InputPortOp` (module-body placement) | `hw::HWModuleOp` block-arg replacement | ModulePatterns / PortPatterns | M5 emits the SSA-Value-bearing form inside `nsl::ModuleOp`'s body so transfers reach the port via SSA. Lowered by replacing every use of the op's result with the `hw::HWModuleOp` block-arg of the corresponding name |
| `nsl::OutputPortOp` (declare-body placement) | (consumed during ModuleOp lowering) | ModulePatterns / PortPatterns | Materialises one output port on the `hw::HWModuleOp`, named `name`, typed `i<W>` |
| `nsl::OutputPortOp` (module-body placement) | `hw::OutputOp` operand wiring | ModulePatterns / PortPatterns | M5 emits the in-module form so `nsl::TransferOp` `%dst` operands have a valid SSA Value. Lowered by tracking writes to the op's result and routing them to the `hw::OutputOp` of the resulting `hw::HWModuleOp` |
| `nsl::InoutPortOp` (declare-body placement) | (consumed during ModuleOp lowering) | ModulePatterns / PortPatterns | Materialises one inout port on the `hw::HWModuleOp`. Direction-marker semantics per CIRCT `hw::ModulePort`'s inout shape |
| `nsl::InoutPortOp` (module-body placement) | bidirectional `hw` port wiring | ModulePatterns / PortPatterns | Read + write paths both wired through the corresponding inout port of the resulting `hw::HWModuleOp` |
| `nsl::SubmoduleOp` (singleton form) | `circt::hw::InstanceOp` | ParamPatterns | Array form already exploded by M5's `NSLExplodeSubmodArrayPass` |
| `nsl::ParamIntOp` | `hw::InstanceOp` parameter wire (i32 typed) | ParamPatterns | S16 — only on HDL-bound `hw::InstanceOp`s |
| `nsl::ParamStrOp` | `hw::InstanceOp` parameter wire (string typed) | ParamPatterns | S16 — only on HDL-bound `hw::InstanceOp`s |
| `nsl::RegOp` (no `interface`) | `circt::seq::FirRegOp` | StatePatterns | Async, active-HIGH reset (Q2 → C, polarity corrected by PR #14 Round-1 review per `nsl_lang.ebnf` §15 line 820's `p_reset` prefix); reset value = `nsl.reg` initializer |
| `nsl::RegOp` (with `interface` modifier, S20) | `circt::seq::CompRegOp` | StatePatterns | User-declared clock/reset names + polarities |
| `nsl::WireOp` | `circt::hw::WireOp` | StatePatterns | |
| `nsl::MemOp` | `circt::seq::FirMemOp` | StatePatterns | Depth + width preserved; init values per S24 zero-fill |
| `nsl::TransferOp` (combinational, `=`) | direct value substitution via `comb::*` ops | StatePatterns | No standalone CIRCT op — RHS SSA value flows to the LHS use sites |
| `nsl::ClockedTransferOp` (`:=`) | `seq::FirRegOp` data-input write | StatePatterns | Write feeds the FirRegOp's data operand |
| `nsl::AltOp` | nested `comb::MuxOp` chain (right-assoc) | ControlPatterns | S13 priority; source order = priority order |
| `nsl::AnyOp` | per-target `comb::OrOp` of `comb::MuxOp(cond, val, 0)` per case | ControlPatterns | S13 parallel |
| `nsl::IfOp` (statement, wire LHS) | `comb::MuxOp` | ControlPatterns | comb-only |
| `nsl::IfOp` (statement, reg LHS) | `seq::FirRegOp(data = comb::MuxOp(%cond, %new, %prev))` | ControlPatterns | Q3 → A: mux-on-data |
| `nsl::ProcOp` (with `nsl::StateOp` children) | `circt::fsm::MachineOp` | FSMPatterns | `initial_state` from `nsl::FirstStateOp` |
| `nsl::SeqOp` (inside `nsl::FuncOp`) | `circt::fsm::MachineOp` with auto-generated `seq_N` states | FSMPatterns | One state per goto label |
| `nsl::StateOp` | `circt::fsm::StateOp` (child of MachineOp) | FSMPatterns | One per `nsl::StateOp`; body is converted region |
| `nsl::FirstStateOp` | (consumed during ProcOp lowering as the `initial_state` attr value) | FSMPatterns | Not a standalone target |
| `nsl::GotoOp` (state form, S25) | `circt::fsm::TransitionOp` | FSMPatterns | Source state → target state |
| `nsl::GotoOp` (label form, inside seq) | `circt::fsm::TransitionOp` (state-name `seq_N`) | FSMPatterns | |
| `nsl::FinishOp` / `nsl::FinishMethodOp` | `circt::fsm::TransitionOp` to synthetic `__sink__` state | FSMPatterns | One sink per MachineOp |
| `nsl::CallOp` (target = `func_in`) | combinational inline + `hw::WireOp` named `<func>_valid` | ControlPatterns | Valid signal materialized |
| `nsl::CallOp` (target = `proc_name`) | `circt::fsm::TransitionOp` to target's initial state | FSMPatterns | |
| `nsl::SimDisplayOp` | `circt::sv::FWriteOp` inside shared `sv::IfDefOp "SIMULATION"` | SimPatterns | |
| `nsl::SimFinishOp` | `circt::sv::FinishOp` inside SIMULATION ifdef | SimPatterns | |
| `nsl::SimInitOp` | `circt::sv::InitialOp` inside SIMULATION ifdef | SimPatterns | |
| `nsl::SimDelayOp` | `circt::sv::DelayOp` inside SIMULATION ifdef | SimPatterns | |
| S29 `_init` block | `circt::sv::InitialOp` inside SIMULATION ifdef | SimPatterns | Q1-specify-time → B; sim-only |
| `nsl::AddOp` / `SubOp` / `MulOp` | `comb::AddOp` / `SubOp` / `MulOp` | ArithPatterns | |
| `nsl::EqOp` / `NeOp` | `comb::ICmpOp` predicate `eq` / `ne` | ArithPatterns | |
| `nsl::LtOp` / `LeOp` / `GtOp` / `GeOp` | `comb::ICmpOp` predicate `slt` / `sle` / `sgt` / `sge` (signed) or `ult` / `ule` / `ugt` / `uge` (unsigned) | ArithPatterns | Signedness from M3 typed AST |
| `nsl::AndOp` / `OrOp` / `XorOp` | `comb::AndOp` / `OrOp` / `XorOp` | BitOpPatterns | |
| `nsl::ShlOp` / `ShrOp` | `comb::ShlOp` / `ShrUOp` | BitOpPatterns | Logical (unsigned) right shift |
| `nsl::LandOp` / `LorOp` | `comb::AndOp` / `OrOp` (operands width-1) | BitOpPatterns | |
| `nsl::NotOp` | `comb::XorOp %a, all-ones` | BitOpPatterns | |
| `nsl::NegOp` | `comb::SubOp 0, %a` | BitOpPatterns | |
| `nsl::LnotOp` | `comb::ICmpOp eq %a, 0` | BitOpPatterns | |
| `nsl::ReduceAndOp` | `comb::ICmpOp eq %a, all-ones` | BitOpPatterns | |
| `nsl::ReduceOrOp` | `comb::ICmpOp ne %a, 0` | BitOpPatterns | |
| `nsl::ReduceXorOp` | `comb::ParityOp` | BitOpPatterns | |
| `nsl::SignExtendOp` | `comb::ConcatOp(replicate %a[msb] × N, %a)` | BitOpPatterns | Q1 → A: comb-only (no `hwarith.cast`) |
| `nsl::ZeroExtendOp` | `comb::ConcatOp(zeros, %a)` | BitOpPatterns | |
| `nsl::MuxOp` (3-input) | `comb::MuxOp` | BitOpPatterns | |
| `nsl::ConcatOp` (variadic) | `comb::ConcatOp` (variadic) | BitOpPatterns | |
| `nsl::ExtractOp` (lowBit attr + result-type-derived width) | `comb::ExtractOp` | BitOpPatterns | |
| `nsl::RepeatOp` | `comb::ReplicateOp` (or N-fold `comb::ConcatOp`) | BitOpPatterns | |
| `nsl::StructuralGenerateOp` | (must NOT be present at M6 input) | — | Eliminated by M5's `NSLExpandGeneratePass`; if present, fail-fast |
| `nsl::FireProbeOp` (S27 marker) | (intentionally absent — sentinel for a later step) | — | NO M6 lowering pattern by design (§10 row 1214 — "marker op lowered later to a 1-bit tap"); fail-fasts under `applyFullConversion` if it survives M5 unresolved. The lowering target — a 1-bit tap on the `fsm.machine`'s state-encoding bits — needs the FSM-to-Seq conversion already to have run, so it belongs to a post-M6 step (M7-or-later). Excluded from §2's bijection rule and from coverage_guard's enforcement set. |

---

## §2. Conversion-pattern bijection rule

Every row of §1 above (excluding the design-§10 "consumed during"
rows and the "must NOT be present" row for `StructuralGenerateOp`,
and the "intentionally absent — sentinel" row for
`FireProbeOp`) MUST have:

1. One `OpConversionPattern<T>` registered under the family named
   in column 3 (Pattern family).
2. One lit fixture pair (`<op>.nsl` + `<op>.mlir.expected`) under
   `test/Lower/circt/<family-or-axis>/`.
3. One row in this contract.

The CI coverage guard (FR-033, `coverage_guard.cmake`) enforces
items 1 and 2 mechanically; item 3 is enforced by code review
(any PR touching the registered-pattern set must touch this
contract too). The two excluded rows
(`StructuralGenerateOp` invariant violation;
`FireProbeOp` sentinel) are tracked in `tasks.md` "Post-
implementation triage" deferred-work catalogue.

---

## §3. `nsl::ModuleOp` + `nsl::DeclareOp` → `hw::HWModuleOp`
        port-list derivation

**Source**: pair of `nsl::ModuleOp @M` and its sibling
`nsl::DeclareOp @M` (matched by literal `pair_name`-equals-`sym_name`)
in the same parent `mlir::ModuleOp` (the project-level outer module).

> **Pairing mechanism note (post-merge M4-amendment #9)**:
> `nsl::DeclareOp` carries `pair_name` (a `SymbolNameAttr`), NOT
> `sym_name`. Using the magic `sym_name` would collide with the
> sibling `nsl::ModuleOp @M`'s `Symbol`-trait registration in the
> parent `mlir::ModuleOp`'s SymbolTable (per
> `mlir/IR/SymbolTable.h`'s "any op carrying a `sym_name`
> StringAttr is uniqueness-checked, regardless of `Symbol`-trait
> inheritance" rule). The ModuleOp pattern walks the parent
> `mlir::ModuleOp`'s direct children and matches by literal name
> string (`pair_name` vs `sym_name`).

**Target**: single `circt::hw::HWModuleOp` whose port list is
derived from the `nsl::DeclareOp` body's port-info children.

**Source-of-truth**: the declare body's child ops
(`nsl::InputPortOp` / `nsl::OutputPortOp` / `nsl::InoutPortOp`).
Each child carries `StrAttr:$name` and a `NSL_AnyBits:$result`
whose result type IS the port's `!nsl.bits<W>` width. The pattern
walks the body in source order to preserve port ordering.

**In-module-body counterpart**: M5 emits the SAME port-info ops
inside `nsl::ModuleOp`'s body too (the dual-placement rule per
amendment-#9 follow-on; SSA-dominance forces this — a sibling-of-
module `nsl::DeclareOp`'s body Values can't dominate a transfer
nested inside `nsl::ModuleOp > nsl::FuncOp > nsl::TransferOp`).
The ModuleOp pattern rewrites each in-module port-info op:
`nsl::InputPortOp` → block-arg substitution on the resulting
`hw::HWModuleOp`'s entry block (every use of the op's result is
replaced with the corresponding block-arg); `nsl::OutputPortOp`
→ output-port wiring (writes to the op's result are tracked and
routed to a single `hw::OutputOp` at the end of the body);
`nsl::InoutPortOp` → bidirectional-port wiring.

**Port-list rules** (frozen):

1. Each `nsl::InputPortOp` in the declare body becomes one input
   port on the `hw::HWModuleOp`, named per the op's `name` attr
   and typed `iW` (where `W` = the op's result `!nsl.bits<W>`).
2. Each `nsl::OutputPortOp` in the declare body becomes one
   output port, same shape.
3. Each `nsl::InoutPortOp` in the declare body becomes one
   inout port, same shape.
4. Each `func_in` declaration's named result (`func_in F : R[W];`
   where `R` is the return-name) becomes one output port on the
   `hw::HWModuleOp`, named `R` and typed `iW`. The function's
   own arguments become input ports if any. The function's valid
   signal is materialized as one output port named `F_valid`,
   typed `i1`. (Source: the `nsl::FuncInOp` ops inside
   `nsl::ModuleOp`'s body — those carry `HasParent<"ModuleOp">`,
   not `HasParent<"DeclareOp">`, per the M5 visitor's split-by-
   direction discipline.)
5. Each `func_out` declaration's argument list becomes one input
   port per arg, named per declaration; the function's body
   contents (the implementation) lives in the module body, not
   exposed as ports. (Source: `nsl::FuncOutOp` ops inside
   `nsl::ModuleOp`'s body.)
6. **Implicit port additions** (no `interface` modifier present):
   one `m_clock` input port (typed `i1`) and one `p_reset` input
   port (typed `i1`) appended at the END of the input port list
   (Q2 → C convention). Module body's `seq::FirRegOp`s wire their
   clock to `m_clock` (via `seq::ToClockOp`) and the reset operand
   directly to the `p_reset` block-arg (active-HIGH; no
   `comb::ICmpOp` adapter). **Round-1 review correction (PR #14,
   2026-05-04)**: the implicit-port names and reset polarity are
   sourced from `nsl_lang.ebnf` §15 lines 818, 820 (`m_clock` =
   auto-synthesized clock, `p_reset` = auto-synthesized reset
   with `p_` indicating active-HIGH). The previous active-LOW
   `clk` / `rst_n` convention violated the spec.
7. **Explicit `interface` (S20)**: the user names clock(s) and
   reset(s) directly in the `interface` clause. Per post-merge
   M4-amendment 2026-05-05 (#10), the modifier IS surfaced on
   `nsl::DeclareOp` as a pair of `OptionalAttr<StrAttr>` —
   `interface_clock` + `interface_reset`. Both ABSENT means rule 6
   applies (implicit `clk` / `rst_n`); both PRESENT means rule 7
   applies — M6's `lowerOneModule` (`ModulePatterns.cpp`) reads the
   attrs and emits two `i1` input ports named exactly per the user's
   declaration (clock first, then reset), in lieu of the implicit
   pair. The reset name preserves the user's polarity-hint suffix
   verbatim (e.g., `_n`); polarity interpretation belongs to the
   reg-lowering branch (Phase 6 territory). `nsl::RegOp` lowers to
   `seq::CompRegOp` on this path (per `firreg-convention.contract.md`
   §2). Asymmetric presence (one attr set, the other unset) is
   rejected by `DeclareOp::verify()`. Closes T033 fixture XFAIL in
   `test/Lower/circt/module/interface_modifier.nsl`.

**Body region**: the conversion of the `nsl::ModuleOp`'s body,
performed via standard `OpConversionPattern` recursion — every
op inside the module body is rewritten by its corresponding
pattern (including the in-module port-info ops, replaced with
block-arg substitutions / output wiring as documented above).

---

## §4. `nsl::AltOp` priority encoding

**Pattern**: right-associative nested `comb::MuxOp`. For

```mlir
nsl.alt {
  case %condA -> %valA
  case %condB -> %valB
  default -> %dflt
}
```

the lowering produces:

```mlir
%result = comb.mux %condA, %valA, %tmp_after_A
%tmp_after_A = comb.mux %condB, %valB, %dflt
```

(IR shape; SSA naming is `mlir-opt`'s default convention.)

Source order in the `nsl::AltOp`'s region IS the priority order;
case A wins over case B wins over default (S13 priority).

If no `default` is present, the fallback is:
- For wire LHS: `hw::ConstantOp` of the LHS width, value 0.
- For reg LHS: `seq::FirRegOp` reading the previous state (the
  reg's own SSA value — the natural `seq.read`-equivalent).

---

## §5. `nsl::AnyOp` parallel encoding

**Round-1 review correction (PR #14, 2026-05-04)**: the M6
`lowerAnyOp` previously funnelled cases through the same priority
chain as `lowerAltOp`, producing nested `comb::MuxOp` updates that
violated S13 parallel semantics (last-case-wins for shared targets
when multiple cases matched). The corrected implementation builds
per-target accumulators and emits the parallel-OR shape below.

**Pattern**: per-target `comb::OrOp` of `comb::MuxOp` envelopes.
For each target wire/reg LHS in the `nsl::AnyOp`'s region:

```mlir
%lhs = comb.or
       %caseA_contribution,
       %caseB_contribution,
       …
%caseA_contribution = comb.mux %condA, %valA_to_lhs, 0
%caseB_contribution = comb.mux %condB, %valB_to_lhs, 0
```

S13 parallel semantics: every matching case fires; non-matching
cases contribute zero (which, OR'd with anything, is identity).
The fixture `test/Lower/circt/control/any_parallel.nsl` is the
canonical regression for this rule.

---

## §6. `nsl::ProcOp` / `nsl::StateOp` → `fsm::MachineOp`

**Pattern**:

```mlir
nsl.proc @P {
  nsl.first_state @s0
  nsl.state @s0 { ... goto @s1 ... }
  nsl.state @s1 { ... }
}
```

lowers to:

```mlir
fsm.machine @P attributes { initial_state = @s0 } {
  fsm.state @s0 {
    fsm.output %... : ...
    fsm.transitions {
      fsm.transition @s1 ...
    }
  }
  fsm.state @s1 {
    fsm.output %... : ...
    fsm.transitions { ... }
  }
}
```

`nsl::FinishOp` inside any state body becomes
`fsm.transition @__sink__` (one synthetic sink state per
MachineOp). The state body's converted region content lives in
the `fsm::StateOp`'s output / transitions sub-regions per CIRCT's
`fsm` dialect contract.

---

## §7. Sim-task ifdef sharing rule

A given `hw::HWModuleOp` body has at most ONE `sv::IfDefOp` whose
condition is `"SIMULATION"`. All sim-task lowerings (`nsl::Sim*Op`
plus the S29 `_init` block) insert into this shared block. The
ifdef's relative position within the module body is at the end
of the body (after all synthesizable ops); within the ifdef,
ops are in source order of their `nsl::*` predecessors.

---

## §8. Dependencies (this contract)

- M4 dialect contract (every `nsl::*` op definition):
  [`specs/007-m4-mlir-dialect/contracts/dialect-api.contract.md`](../../007-m4-mlir-dialect/contracts/dialect-api.contract.md)
- Design §10 (the source-of-truth mapping table):
  [`docs/design/nsl_compiler_design.md`](../../../docs/design/nsl_compiler_design.md)
  lines 1235–1305
- Spec FR-006 + FR-007 + FR-008 + FR-013–FR-022 (per-row
  enforcement):
  [../spec.md](../spec.md)

A mismatch between this contract and design §10 is a Principle VII
coupling violation; resolve by amending both in one PR.
