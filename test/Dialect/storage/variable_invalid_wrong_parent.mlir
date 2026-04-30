// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// RUN: nsl-opt --verify-diagnostics %s
//
// FR-013 / FR-019 invariant: `nsl.variable` parent ∈ {`nsl.module`,
// `nsl.func`} (variadic `HasParent` per data-model §2.2). Placing it
// directly under the builtin `mlir::ModuleOp` violates the trait.
// Expects standard MLIR trait diagnostic substring "expects parent op"
// once T099 lands.

// expected-error@+1 {{expects parent op}}
%v = nsl.variable "v" : !nsl.bits<8>
