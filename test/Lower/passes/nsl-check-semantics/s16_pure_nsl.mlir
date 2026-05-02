// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// RUN: nsl-opt --verify-diagnostics --split-input-file -nsl-check-semantics %s
//
// M5 US4 / FR-018 — sensitive-`Sn` re-check S16 per
// `pass-pipeline.contract.md` §3 row S16:
//
//   "S16 | param_int / param_str ref survives in pure-NSL module
//    | error: parameter '@<name>' meaningful only for V/V/SC
//    submodules"
//
// At M5's frozen 79-op surface, NO `nsl::*` op carries an operand-
// side `FlatSymbolRefAttr` referencing a `nsl.param_int` —
// `NSLResolveParamsPass` (slot 1) is intentionally a no-op for
// pure-NSL inputs (the param ops outlive the pipeline; M7 consumes
// them at Verilog instantiation). The "ref survives" condition is
// therefore vacuous at M5.
//
// However, **the OP itself surviving is a meaningful S16 signal**:
// per S16 + grammar §3.1, `param_int` / `param_str` are meaningful
// ONLY when a Verilog/VHDL/SystemC submodule consumes them. A
// pure-NSL module that declares a `param_int` but never instantiates
// any V/V/SC submodule is the S16 violation shape we re-check here.
// (Heuristic: top-level `nsl.param_int` exists AND no `nsl.submodule`
// op reaches a non-NSL template — at M5, all submodules are pure-NSL,
// so any surviving `param_int` qualifies.)

// expected-error@+1 {{parameter '@N' meaningful only for V/V/SC submodules}}
nsl.param_int @N = 8

nsl.module @S16PureNsl {
}
