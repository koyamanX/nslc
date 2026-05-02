// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// RUN: nsl-opt -nsl-resolve-params %s | FileCheck %s
//
// M5 US2 / FR-013 — `NSLResolveParamsPass` (slot 1 of the 6-slot
// pipeline per `pass-pipeline.contract.md` §2). The pass walks the
// top-level `nsl::ModuleOp`, builds a `paramMap_<StringRef, int64_t>`
// from every `nsl.param_int` op, then scans every reachable op for
// operand-side `FlatSymbolRefAttr` references that match a paramMap
// key and substitutes them with `nsl.constant <value> :
// !nsl.bits<N>` SSA values. After substitution the `nsl.param_int`
// op is erased.
//
// Cited design: `specs/008-m5-structural-passes/spec.md` FR-013;
// `data-model.md` §7.2; the M4 op surface frozen at 79 ops post-
// amendment-#4 (which added `nsl.param_int` / `nsl.param_str`).
//
// **STATUS at HEAD `7915cda`**: at the M5-frozen dialect surface,
// NO `nsl::*` op carries a `FlatSymbolRefAttr` slot pointing at a
// param symbol — `nsl.submodule.templateRef` points at sibling
// modules; `nsl.func_call.callee` points at funcs; `nsl.fire_probe.
// target` points at control terminals. So this pass has nothing to
// substitute against on PURE-NSL inputs at M5; param eagerness is
// performed at the AST→MLIR visitor stage instead (see
// `visit(TopLevelParamDecl)` + the visitor's `paramTable_` lookup
// in `visit(StructuralGenerate)` for generate bounds). The pass
// runs as a registered slot but its body is a defensive walk: if a
// future op grows a param-ref slot, this pass picks it up
// automatically. For M5 this fixture asserts the no-op invariant —
// `nsl.param_int @N = 8` survives the pass (param ops outlive the
// pipeline at M5; M7 will consume them when `nsl.submodule` lowers
// to Verilog instantiation with `param_int <N>` instance args).

// CHECK: nsl.param_int @N = 8
nsl.param_int @N = 8

// CHECK-LABEL: nsl.module @ParamHost
nsl.module @ParamHost {
}
