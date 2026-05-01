// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// XFAIL: *
// RUN: nsl-opt --verify-diagnostics --split-input-file -nsl-check-semantics %s
//
// M5 US4 / FR-018 — sensitive-`Sn` re-check S20 per
// `pass-pipeline.contract.md` §3 row S20:
//
//   "S20 | submod-array element lacks parent's interface modifier
//    binding | error: submodule '<name>[<i>]' missing interface
//    modifier from parent"
//
// **DEFERRED at M5 (post-M6 surface)** — interface-modifier
// bindings are an M6 surface (`hw.instance` modifier propagation
// at port-binding time). At M5's frozen 79-op surface, the dialect
// has no representation for "interface modifier on a submodule-
// array element"; the structural shape that would trigger this
// re-check is unrepresentable.
//
// When M6 adds `nsl.submod_iface_bind` (or whatever the eventual
// op is), this re-check helper walks every submod-array element
// (`nsl.submodule` with `_<index>` suffix from the explode pass)
// and asserts the iface-bind op exists for it. Until then, this
// fixture is XFAIL'd.

nsl.module @S20Iface {
  // expected-error@+1 {{submodule 'inst[0]' missing interface modifier from parent}}
  nsl.submodule @inst : @SUB
}

nsl.module @SUB {
}
