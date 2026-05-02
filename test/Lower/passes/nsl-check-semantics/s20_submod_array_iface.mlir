// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// RUN: nsl-opt --verify-diagnostics --split-input-file -nsl-check-semantics %s
//
// M5 US4 / FR-018 — sensitive-`Sn` re-check S20 per
// `pass-pipeline.contract.md` §3 row S20:
//
//   "S20 | submod-array element lacks parent's interface modifier
//    binding | error: submodule '<name>[<i>]' missing interface
//    modifier from parent"
//
// **VACUOUS ON M5 SURFACE — converted to a no-violation PASS case.**
// Interface-modifier bindings are an M6 surface (`hw.instance`
// modifier propagation at port-binding time). At M5's frozen 79-op
// surface, the dialect has no representation for "interface
// modifier on a submodule-array element"; the structural shape
// that would trigger this re-check is unrepresentable. The re-
// check helper in `NSLCheckSemanticsPass` is a documented no-op
// stub for S20 on M5.
//
// When M6 adds `nsl.submod_iface_bind` (or whatever the eventual
// op is), this fixture pivots back into a fail-case shape: the
// helper walks every submod-array element (`nsl.submodule` with
// `_<index>` suffix from the explode pass) and asserts the iface-
// bind op exists for it. Until then, this fixture asserts the
// pass accepts the structurally-clean shape without a (vacuously-
// impossible-to-trigger) diagnostic.

nsl.module @S20Iface {
  // Single (non-array) submodule. Post-explode-submod-array there
  // are zero `_<index>`-suffixed elements, so S20 is structurally
  // unreachable on this input.
  nsl.submodule @inst : @SUB
}

nsl.module @SUB {
}
