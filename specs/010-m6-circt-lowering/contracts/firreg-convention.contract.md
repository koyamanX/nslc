<!-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception -->

# Contract: `seq.firreg` reset polarity + `nsl.if`-over-reg lowering conventions

**Branch**: `010-m6-circt-lowering` | **Date**: 2026-05-04
**Spec**: [../spec.md](../spec.md) (Q2 → C, Q3 → A in `## Clarifications`)
**Plan**: [../plan.md](../plan.md)
**Research**: [../research.md](../research.md) §§4–5

This contract pins the two M6 register-lowering conventions that
shape every output `seq.firreg` and that downstream M7 audited-
corpus golden VCDs rely on. Changing either convention is a
project-wide impact requiring a contract amendment + audited-
corpus VCD regeneration; treat any deviation as a blocker
finding in PR review.

---

## §1. Default reset convention (no `interface` modifier)

**Round-1 review correction (PR #14, 2026-05-04)**: this section
previously specified `clk` + `rst_n` (active-LOW reset, the user
finding called this out as a violation of the NSL spec). The
NSL grammar (`nsl_lang.ebnf` §15 lines 818, 820) reserves
`m_clock` (auto-synthesized clock) and `p_reset` (auto-synthesized
reset). The `p_` prefix indicates **active-HIGH** polarity — the
straightforward reading of the prefix; an `n_reset` would denote
active-LOW. The implementation has been corrected to match.

**Rule**: When an `nsl::ModuleOp`'s paired `nsl::DeclareOp` does
NOT contain the S20 `interface` modifier, every `nsl::RegOp`
inside that module lowers to a `circt::seq::FirRegOp` with:

- **Async reset**: `seq::FirRegOp`'s `async_reset` flag is set;
  the reset operand is the raw `%p_reset` block-arg — i.e.,
  reset fires when `p_reset` is HIGH (1).
- **Active-HIGH**: the FirRegOp's reset operand is the `%p_reset`
  block-arg directly; NO `comb::ICmpOp` adapter is interposed.
  (Round-1 fix: prior code did `comb.icmp eq %rst_n, 0` which
  encoded active-LOW. That adapter has been removed.)
- **Reset value**: `nsl::RegOp`'s initializer literal (`reg r[8] = K;`
  → `K`; bare `reg r[8];` → 0 per S2/S23). Width matches the
  reg's data type.
- **Clock**: connected to the implicit `m_clock` port (positive-edge
  triggered) via `seq::ToClockOp`.
- **Implicit ports** on the enclosing `hw::HWModuleOp`: `m_clock`
  and `p_reset`, both `i1`, appended at the end of the input port
  list in that order. The names come from `nsl_lang.ebnf` §15.

**Cross-module consistency**: every module in a compilation unit
that lacks the `interface` modifier shares the same
`(m_clock, p_reset)` naming convention. Modules using non-standard
names must opt in via the explicit S20 `interface` modifier.

---

## §2. Explicit `interface` modifier (S20)

**Rule**: When the `nsl::DeclareOp` carries the post-merge
M4-amendment-#10 `interface_clock` + `interface_reset`
`OptionalAttr<StrAttr>` pair (both PRESENT), the user has named
clock(s) and reset(s) explicitly. M6 honours those names verbatim:

- The IR-level signal is the dialect attribute pair; M6's port-list
  derivation (`buildPortInfo` in `ModulePatterns.cpp`) reads the
  attrs directly — no reach-back into Sema's symbol table is needed
  (Principle II layering; the attrs ARE the IR contract).
- Each clock named in `interface_clock` becomes one `i1` input port
  on the `hw::HWModuleOp`, named per the user's declaration.
- Each reset named in `interface_reset` becomes one `i1` input
  port, named per the user's declaration; polarity (active-high or
  active-low) is hinted by the user's chosen suffix (e.g., `_n` for
  active-low). The dialect attribute preserves the raw S20 syntax;
  polarity interpretation is owned by the reg-lowering branch
  (Phase 6 territory).
- `nsl::RegOp` lowers to **`circt::seq::CompRegOp`** (not
  FirRegOp) with the explicit clock and reset operands wired to
  the user-named ports.
- The default `(m_clock, p_reset)` ports from §1 are NOT auto-added
  in this case — the user took ownership of the clock/reset surface.
- Asymmetric presence (one attr set, the other unset) is rejected
  by `DeclareOp::verify()` per amendment-#10 since S20 mandates
  BOTH names whenever the modifier appears.

`seq::CompRegOp` is preferred over `seq::FirRegOp` for this path
because CompRegOp's clock + reset operands are explicit
(matching the user's explicit interface declaration), while
FirRegOp uses implicit module-level clock + reset wires.

**Phase 4 status (M6 amendment-#10 commit)**: only the port-naming
half of this rule is implemented — `lowerOneModule` reads the attrs
and emits user-named `i1` clock + reset ports. The `nsl::RegOp` →
`seq::CompRegOp` reg-lowering branch lands in Phase 6 (US4 state-
family); Phase 4 doesn't lower `nsl::RegOp` at all.

---

## §3. Conditional reg-update: mux-on-data (Q3 → A)

**Rule**: An `nsl::IfOp` whose LHS targets a `nsl::RegOp` lowers
to a `comb::MuxOp` placed in the data path of the reg's
`seq::FirRegOp` (or `seq::CompRegOp` on the explicit-interface
path):

```mlir
%prev = <seq.firreg's current SSA value>
%new_data = comb.mux %cond, %new_value, %prev
seq.firreg %r, %m_clock, %new_data, ...   // §1 reset wiring
```

For chained `nsl::IfOp`s (`if (a) { if (b) { reg = x; } else
{ reg = y; } }`), the nesting becomes nested `comb::MuxOp`s
right-to-left:

```mlir
%inner_data = comb.mux %b, %x, %y
%new_data   = comb.mux %a, %inner_data, %prev
```

**One `seq::FirRegOp` per `nsl::RegOp`**, regardless of how many
`nsl::IfOp`s the reg participates in. Conditionality lives
entirely in the data path; the register clock-edge happens every
cycle.

---

## §4. What is NOT done at M6

The following alternative implementations are **prohibited** at
M6 (each would deviate from Q2/Q3 conventions):

- **`seq::CompRegOp` with enable port** for conditional updates:
  reserved for the explicit-`interface`-modifier path only;
  never used as a fallback in the no-`interface` path.
- **`seq::ClockGate` op** anywhere: not introduced at M6.
- **`sv::AlwaysFFOp` or `sv::IfOp`** in synthesizable IR (sim-only
  ifdef wrapping is a separate `sv::IfDefOp` — different op):
  not introduced at M6.
- **Sync reset, active-low reset, or async-active-low reset on the
  no-`interface` path**: prohibited; only async-active-HIGH (the
  `p_reset` polarity per the `p_` prefix convention frozen at PR
  #14 review-#0; corrected from the original Q2 → C "active-low"
  pin).
- **Per-flag (`-emit=hw --reset=sync`) configurability**:
  prohibited; the convention is project-wide.

---

## §5. Lit-fixture pinning

**Round-1 review correction (PR #14)**: the canonical reg shape
now uses `m_clock` + `p_reset` (active-HIGH) per §1.

The fixtures `test/Lower/circt/state/reg_basic.nsl` (and its
sibling `.mlir.expected`) MUST exhibit:

```mlir
hw.module @M(%m_clock: i1, %p_reset: i1, ...) -> (...) {
  %clk = seq.to_clock %m_clock
  %r = seq.firreg %r_next clock %clk reset async %p_reset,
                  %rst_value : i8
  ...
}
```

There is NO `comb::ICmpOp eq %p_reset, 0` adapter; the active-HIGH
polarity is implicit in the FirRegOp's reset operand being the
raw `p_reset` block-arg.

The fixture `reg_with_init.nsl` MUST exhibit a `hw.constant 42`
hosting the reset value for `reg r[8] = 42;`. The fixture
`reg_with_interface.nsl` MUST exhibit `seq.compreg` with explicit
clock/reset operands sourced from the user's `interface`
declaration. The fixture `if_reg_lhs.nsl` MUST exhibit `comb.mux`
on the data input — with the FALSE arm referencing the firreg's
own result (`%r`), NOT the reset constant (Round-1 Finding #9
fix; an unwritten reg holds its previous value). The fixture
`chained_if_reg.nsl` MUST exhibit nested `comb.mux` (§3 second
example) with the inner mux's prev arm as `%r` and the outer
mux's prev arm as the inner mux result. The fixture
`reg_unwritten_holds_prev.nsl` is the dedicated regression for
Finding #9. The fixture `wire_multi_write.nsl` is the dedicated
regression for Finding #10 (deferred-emit `hw::WireOp` materialised
post-walk after all producers exist).

These fixtures are the canonical tie-breakers: a contributor
proposing a deviation from §1–§3 must update them, which is a
visible audit trail in code review.

---

## §6. Audited-corpus alignment forecast (M7 dependency)

**Round-1 review correction (PR #14)**: the polarity choice
(active-HIGH) follows the NSL spec's `p_reset` reserved-keyword
prefix (`p_` = positive). M7's golden VCD comparison will compare
`nslc -emit=hw … | circt-opt --convert-fsm-to-sv --lower-seq-to-sv …`
output against the audited corpus's reference Verilog. The
audited NSL projects' reference Verilog patterns must be
re-checked against the M6 emit at M7-time; if they use
`always @(posedge clk or negedge rst_n)` (active-LOW), M7's
test-bench wrapper will need to invert the reset signal at the
DUT boundary OR the audited projects' reset wiring will be
re-generated through the same `nslc -emit=hw` flow (which guarantees
self-consistency).

This forecast is informational — M7 owns the actual audited-
corpus regression — but it justifies why the M6 convention is
worth pinning explicitly rather than leaving to per-fixture
choice.

---

## §7. Cross-references

- Spec Q2 + Q3 in `## Clarifications`:
  [../spec.md](../spec.md)
- Research §§4–5 (rationale + alternatives):
  [../research.md](../research.md)
- Design §10 line 1209 (`nsl.reg` → `seq.firreg`/`seq.compreg`
  branching):
  [`docs/design/nsl_compiler_design.md`](../../../docs/design/nsl_compiler_design.md)
- Design §10 line 1217 (`nsl.if` reg-LHS lowering — the design
  text "conditional reg-enable" is now operationalised as
  mux-on-data per Q3):
  [`docs/design/nsl_compiler_design.md`](../../../docs/design/nsl_compiler_design.md)
- Constitution Principle III (stock CIRCT; no hand-rolled
  reg-inference):
  [`.specify/memory/constitution.md`](../../../.specify/memory/constitution.md)
  §279 ff.
