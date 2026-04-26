<!-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception -->

# NSL Examples

A small, single-file gallery of NSL programs distilled from the
[NSL Reference Manual ver. 1.4E](https://www.overtone.co.jp/Document/NSL_Language_Reference_ver1.4E.pdf)
and the [NSL Tutorial (English) 2015-10-15](https://www.overtone.co.jp/Document/NSLTUT20151006_E.pdf).
Each file is **self-contained** (no `#include`, no companion file) and
short enough to use as a regression test for individual language features.

Every example is annotated with comments that cite the relevant
**semantic constraints (`Sn`)** and **parser notes (`Nn`)** from
[`docs/spec/nsl_lang.ebnf`](../docs/spec/nsl_lang.ebnf). Use `grep -n
"(S<n>)" docs/spec/nsl_lang.ebnf` to jump to the constraint definition.

## Index

| #  | File | Topic | Key constraints / notes |
|----|------|-------|-------------------------|
| 01 | [`01_hello.nsl`](01_hello.nsl) | Simulation `_display` / `_finish` | S17, S29, N11 |
| 02 | [`02_and_gate.nsl`](02_and_gate.nsl) | Pure combinational module | S2, S3, N2 |
| 03 | [`03_register.nsl`](03_register.nsl) | Synchronous 8-bit register | S2, S3, S23 |
| 04 | [`04_counter.nsl`](04_counter.nsl) | 4-bit free-running counter | S3, S23, Ref §5 |
| 05 | [`05_alu.nsl`](05_alu.nsl) | 4-bit ALU dispatched by `alt` | S13, N2 |
| 06 | [`06_alt_vs_any.nsl`](06_alt_vs_any.nsl) | `alt` (priority) vs `any` (parallel) | S3, S12, S13 |
| 07 | [`07_function.nsl`](07_function.nsl) | `func_in` with return-value port | S4, S5, S22 |
| 08 | [`08_func_self.nsl`](08_func_self.nsl) | Internal `func_self` control terminal | S4, S27 |
| 09 | [`09_proc_seq.nsl`](09_proc_seq.nsl) | `proc` body with `seq` + `goto` label | S7, S19, S21, S25 |
| 10 | [`10_for_loop.nsl`](10_for_loop.nsl) | C-style `for` loop in `seq` | S7, S8, S9, S19 |
| 11 | [`11_while_loop.nsl`](11_while_loop.nsl) | `while` loop in `seq` | S7, S8, S9, S19 |
| 12 | [`12_generate.nsl`](12_generate.nsl) | Structural `generate` (compile-time unroll) | S10, S15 |
| 13 | [`13_fsm.nsl`](13_fsm.nsl) | FSM with `state_name` + `first_state` | S11, S19, S25, S28 |
| 14 | [`14_memory.nsl`](14_memory.nsl) | On-chip `mem` with `{0}` initialiser | S3, S24 |
| 15 | [`15_struct.nsl`](15_struct.nsl) | User `struct` type + `reg` instance | S18, Ref §12 |
| 16 | [`16_bit_ops.nsl`](16_bit_ops.nsl) | Sign-extend / zero-extend / slice / concat | S15, N5, Ref §11 |
| 17 | [`17_concat_lvalue.nsl`](17_concat_lvalue.nsl) | `.{a, b, c} = expr;` LHS form | S12, N3 |
| 18 | [`18_proc_methods.nsl`](18_proc_methods.nsl) | `proc.invoke()` / `proc.finish()` methods | S21, S27, N6 |
| 19 | [`19_param.nsl`](19_param.nsl) | Top-level `param_int` constants | S15, S16, Ref §3.1 |
| 20 | [`20_simulation_tb.nsl`](20_simulation_tb.nsl) | Full simulation testbench | S17, S29, N11 |

## Conventions

* **Single-file**, **no preprocessor includes** — every example parses on
  its own. The preprocessor seam (see
  [`docs/spec/nsl_pp.ebnf`](../docs/spec/nsl_pp.ebnf)) is therefore a no-op
  for this gallery; only `#line` markers (which would be inserted by the
  compiler driver) survive into the parser, and none of the example files
  emit one.
* **One concept per file** — each example focuses on a single
  language feature so it can serve as a focused unit test.
* **Synthesizable by default** — files in this directory are
  synthesizable unless their `declare` carries the `simulation` modifier
  (currently `01_hello.nsl` and `20_simulation_tb.nsl`).

## Cross-reference

For the full grammar and semantic-constraint catalogue see:

* [`docs/spec/nsl_lang.ebnf`](../docs/spec/nsl_lang.ebnf) — grammar
  productions plus semantic constraints `S1`–`S29` and parser notes
  `N1`–`N14`.
* [`docs/spec/nsl_pp.ebnf`](../docs/spec/nsl_pp.ebnf) — preprocessor
  grammar plus notes `P1`–`P13`.
