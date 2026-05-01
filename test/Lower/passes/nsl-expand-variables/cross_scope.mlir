// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// XFAIL: *
// RUN: nsl-opt -nsl-expand-variables %s | FileCheck %s
//
// M5 US3 / FR-015 — `NSLExpandVariablesPass` (slot 3). Fixture
// axis (e) per `spec.md:272-273`:
//
//   "(e) variable declared in a func and consumed in an enclosing
//    proc (cross-scope visibility)."
//
// **DEFERRED — M4 parent-constraint tension**. `nsl.variable`
// allows `ParentOneOf<["ModuleOp", "FuncOp"]>` (NSLOps.td:273),
// so a func-internal variable IS legal at the dialect layer.
// **However**, the post-pass replacement op is `nsl.wire`, which
// has `HasParent<"ModuleOp">` (NSLOps.td:260) — so the natural
// pass output (a wire inside a func) is verifier-rejected. The
// pass would have to either:
//   (a) hoist all per-version wires to module-level (changes
//       naming surface; needs cross-region SSA-Value remap), OR
//   (b) introduce a new func-scope-friendly storage op (M4
//       amendment territory), OR
//   (c) leave func-scope variables un-expanded and let M6 deal
//       with them (defeats FR-015's "Post-pass IR MUST contain
//       zero nsl.variable").
//
// Per the US2 T066/T067 precedent, this is documented as a
// deferred amendment-class blocker. Module-scope variables
// (scenarios 1-3) are the substantive deliverable for US3 ship.
//
// See also `cross_scope.mlir`'s sibling deferral note in
// `tasks.md` line 168.

nsl.module @CrossScope {
  nsl.func @f {
    // Inside a func, the variable is legal at the dialect layer
    // but its post-pass replacement (a wire) is not.
    %v = nsl.variable "v" : !nsl.bits<8>
  }
  // CHECK-NOT: nsl.variable
}
