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

**Rule**: When an `nsl::ModuleOp`'s paired `nsl::DeclareOp` does
NOT contain the S20 `interface` modifier, every `nsl::RegOp`
inside that module lowers to a `circt::seq::FirRegOp` with:

- **Async reset**: `seq::FirRegOp`'s `async_reset` flag is set;
  the reset operand is connected to a `comb::ICmpOp eq %rst_n, 0`
  derived condition — i.e., reset fires when `rst_n` is low.
- **Active-low**: the polarity is encoded by the `comb::ICmpOp
  eq %rst_n, 0` upstream condition; the FirRegOp itself sees a
  1-bit "reset-fires" boolean.
- **Reset value**: `nsl::RegOp`'s initializer literal (`reg r[8] = K;`
  → `K`; bare `reg r[8];` → 0 per S2/S23). Width matches the
  reg's data type.
- **Clock**: connected to the implicit `clk` port (positive-edge
  triggered).
- **Implicit ports** on the enclosing `hw::HWModuleOp`: `clk` and
  `rst_n`, both `i1`, appended at the end of the input port list
  in that order.

**Cross-module consistency**: every module in a compilation unit
that lacks the `interface` modifier shares the same `(clk, rst_n)`
naming convention. Modules using non-standard names must opt in
via the explicit S20 `interface` modifier.

---

## §2. Explicit `interface` modifier (S20)

**Rule**: When the `nsl::DeclareOp` contains an S20 `interface`
modifier, the user has named clock(s) and reset(s) explicitly. M6
honours those names verbatim:

- Each clock named in `interface` becomes one `i1` input port on
  the `hw::HWModuleOp`, named per the user's declaration.
- Each reset named in `interface` becomes one `i1` input port,
  named per the user's declaration; polarity (active-high or
  active-low) is per the user's declaration.
- `nsl::RegOp` lowers to **`circt::seq::CompRegOp`** (not
  FirRegOp) with the explicit clock and reset operands wired to
  the user-named ports.
- The default `(clk, rst_n)` ports from §1 are NOT auto-added in
  this case — the user took ownership of the clock/reset surface.

`seq::CompRegOp` is preferred over `seq::FirRegOp` for this path
because CompRegOp's clock + reset operands are explicit
(matching the user's explicit interface declaration), while
FirRegOp uses implicit module-level clock + reset wires.

---

## §3. Conditional reg-update: mux-on-data (Q3 → A)

**Rule**: An `nsl::IfOp` whose LHS targets a `nsl::RegOp` lowers
to a `comb::MuxOp` placed in the data path of the reg's
`seq::FirRegOp` (or `seq::CompRegOp` on the explicit-interface
path):

```mlir
%prev = <seq.firreg's current SSA value>
%new_data = comb.mux %cond, %new_value, %prev
seq.firreg %r, %clk, %new_data, ...   // §1 reset wiring
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
- **Sync reset, active-high reset, or async-active-high reset on
  the no-`interface` path**: prohibited; only async-active-low.
- **Per-flag (`-emit=hw --reset=sync`) configurability**:
  prohibited; the convention is project-wide.

---

## §5. Lit-fixture pinning

The fixtures `test/Lower/circt/state/reg_basic.nsl` (and its
sibling `.mlir.expected`) MUST exhibit:

```mlir
hw.module @M(%clk: i1, %rst_n: i1, ...) -> (...) {
  %not_rstn = comb.icmp eq %rst_n, 0
  %r = seq.firreg %clk, %r_next, async_reset %not_rstn,
                  reset_value 0 : i8
  ...
}
```

The fixture `reg_with_init.nsl` MUST exhibit `reset_value 42` (or
whatever K) for `reg r[8] = 42;`. The fixture `reg_with_interface.nsl`
MUST exhibit `seq.compreg` with explicit clock/reset operands
sourced from the user's `interface` declaration. The fixture
`reg_with_if.nsl` MUST exhibit `comb.mux` on the data input
(§3). The fixture `reg_with_chained_if.nsl` MUST exhibit nested
`comb.mux` (§3 second example).

These fixtures are the canonical tie-breakers: a contributor
proposing a deviation from §1–§3 must update them, which is a
visible audit trail in code review.

---

## §6. Audited-corpus alignment forecast (M7 dependency)

The audited NSL projects (`cpu16`, `mips32_single_cycle`, etc.)
ship with reference Verilog using `always @(posedge clk or
negedge rst_n)` patterns — i.e., the §1 convention exactly. M7's
golden VCD comparison will compare `nslc -emit=hw … |
circt-opt --convert-fsm-to-seq --lower-seq-to-sv …` output
against those references. Choosing async-active-low at M6 keeps
M7 byte-stable; deviating would force every audited project's
golden to be regenerated.

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
