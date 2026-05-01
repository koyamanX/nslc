// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// RUN: nsl-opt --verify-diagnostics --split-input-file --mlir-very-unsafe-disable-verifier-on-parsing -nsl-check-semantics %s
//
// M5 US4 / FR-018 — sensitive-`Sn` re-check S10 per
// `pass-pipeline.contract.md` §3 row S10:
//
//   "S10 | loop variable still present (slot 2 cleanup failed) |
//    error: 'generate' loop variable '%<name>%' not eliminated by
//    structural expansion"
//
// Trigger condition: a `nsl.structural_generate` op survives to
// slot 6 (slot 2's expand-generate pass would normally have erased
// it). At M5 with the full pipeline running, this should not
// happen — but if a pipeline-skipped pass-standalone invocation
// reaches slot 6 (or a future bug short-circuits slot 2), this
// fixture catches it.
//
// `--mlir-very-unsafe-disable-verifier-on-parsing` permits
// authoring `nsl.structural_generate` containing `nsl.reg` (whose
// parent constraint is `ParentOneOf<["ModuleOp", "ProcOp"]>` —
// `StructuralGenerateOp` is neither, so the verifier rejects the
// pre-pass shape). Same precedent as
// `test/Lower/passes/nsl-expand-generate/literal_bound.mlir`.

nsl.module @S10LoopVarResidue {
  // The residue regex (Step 1) ALSO fires on the inner `nsl.reg`'s
  // `"buf_%i%"` StringAttr — that's an independent valid diagnostic
  // since `%i%` matches the residue regex regardless of whether the
  // enclosing structural_generate also surfaces a separate S10
  // signal. The fixture asserts BOTH (Step 1 residue + Step 2 S10)
  // to lock in the multi-error semantics per
  // `pass-pipeline.contract.md` §4.
  // expected-error@+1 {{'generate' loop variable '%i%' not eliminated by structural expansion}}
  nsl.structural_generate attributes {lower = 0 : i64, upper = 4 : i64, step = 1 : i64, loop_var = "i"} {
    // expected-error@+1 {{unresolved macro splice '%i%' after structural expansion}}
    nsl.reg "buf_%i%" : !nsl.bits<8>
  }
}
