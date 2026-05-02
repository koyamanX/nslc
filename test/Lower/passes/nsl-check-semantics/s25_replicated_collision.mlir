// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// RUN: nsl-opt --verify-diagnostics --split-input-file -nsl-check-semantics %s
//
// M5 US4 / FR-018 — sensitive-`Sn` re-check S25 per
// `pass-pipeline.contract.md` §3 row S25:
//
//   "S25 | replicated-body emits two decls with the same name
//    | error: duplicate declaration '<name>' in replicated 'generate'
//    body"
//
// Post-explode-submod-array (slot 4) and post-expand-generate
// (slot 2), all replicated names should be uniquified per their
// per-iteration index. If a collision survives — e.g., because the
// source `generate` body references `buf_const` with no `%<loop_var>%`
// substitution token — the unrolled bodies all declare the same name.
//
// The hand-rolled fixture below presents the post-pipeline shape:
// two `nsl.reg` ops with identical `name` attribute in the same
// `nsl.module` scope. (The exact pre-expansion source pattern that
// generates this is `generate (i < 2) { reg buf_const; }` — but
// authoring the post-pipeline shape directly is more focused on
// the re-check helper itself.)

nsl.module @S25ReplicatedCollision {
  nsl.reg "buf_const" : !nsl.bits<8>
  // expected-error@+1 {{duplicate declaration 'buf_const' in replicated 'generate' body}}
  nsl.reg "buf_const" : !nsl.bits<8>
}
