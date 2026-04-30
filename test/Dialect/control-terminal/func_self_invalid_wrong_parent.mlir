// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// RUN: nsl-opt --verify-diagnostics %s
//
// FR-013 / FR-019 invariant: `nsl.func_self` parent must be
// `nsl.module` (per data-model §2.3 — `HasParent<"NSL_ModuleOp">`).
// Placing it directly under the builtin `mlir::ModuleOp` violates
// the trait.
// Expects standard MLIR trait diagnostic substring "expects parent op"
// once T099 lands.

// expected-error@+1 {{expects parent op}}
nsl.func_self "fire"() : () -> ()
