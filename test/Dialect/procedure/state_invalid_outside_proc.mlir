// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// RUN: nsl-opt --verify-diagnostics --split-input-file %s
//
// FR-013 / FR-019 invariant: `nsl.state` parent must be one of
// `nsl.module` or `nsl.proc` (per data-model §2.7 + amendment #8 —
// `ParentOneOf<["ModuleOp", "ProcOp"]>`). The legal NSL shapes are:
//   1. `state s { ... }` at module top-level (sibling of `nsl.proc`).
//   2. `nsl.state @s { ... }` inside `nsl.proc @p { ... }` (legacy
//      M4 shape; preserved for backward compatibility).
// Placing it inside `nsl.func` (or any other op) violates the trait.

nsl.module @StateInsideFunc {
  nsl.func @body {
    // expected-error@+1 {{expects parent op}}
    nsl.state @s0 {
    }
  }
}

// -----

// Direct top-level placement (no enclosing `nsl.module`) is also
// rejected: the trait requires `ModuleOp` or `ProcOp` parent.

// expected-error@+1 {{expects parent op}}
nsl.state @s_orphan {
}
