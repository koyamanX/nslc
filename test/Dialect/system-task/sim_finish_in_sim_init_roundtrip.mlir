// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// RUN: nsl-opt %s | FileCheck %s
// RUN: nsl-opt %s | nsl-opt - | FileCheck %s
//
// Post-merge amendment 2026-04-30 #7 round-trip for the
// `_init { _display; _finish; }` shape. Per S29
// (`docs/spec/nsl_lang.ebnf §10` line 1007), `_init { ... }` is the
// canonical placement for simulation-termination idioms; the trait
// `HasParent<"ModuleOp"> → ParentOneOf<["ModuleOp", "SimInitOp"]>`
// relaxation on `nsl.sim_finish` aligns the dialect with NSL grammar.
// Sibling `nsl.sim_display` and `nsl.sim_delay` already accept the
// `SimInitOp` parent (Phase 3 baseline); this fixture exercises the
// new third sim-op (`sim_finish`) inside the same body.

// CHECK-LABEL: nsl.module @SimFinishInInit
nsl.module @SimFinishInInit {
  // CHECK: nsl.sim_init
  nsl.sim_init {
    // CHECK: nsl.sim_display "starting"
    nsl.sim_display "starting"
    // CHECK: nsl.sim_delay 1
    nsl.sim_delay 1
    // CHECK: nsl.sim_finish "done"
    nsl.sim_finish "done"
  }
}
