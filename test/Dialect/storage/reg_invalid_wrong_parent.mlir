// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// RUN: nsl-opt --verify-diagnostics %s
//
// FR-013 / FR-019 invariant: `nsl.reg` parent ∈ {`nsl.module`,
// `nsl.proc`} (variadic `HasParent` per data-model §2.2). Placing
// the op directly under the builtin `mlir::ModuleOp` violates the
// trait.
// Expects standard MLIR trait diagnostic substring "expects parent op"
// once T099 lands.

// expected-error@+1 {{expects parent op}}
%q = nsl.reg "q" : !nsl.bits<8> = 0
