// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// RUN: nsl-opt -nsl-check-semantics %s | FileCheck %s
//
// M5 US4 / FR-018 — `NSLCheckSemanticsPass` clean-baseline per
// `residue-detection.contract.md` §8 last row + the FALSE-POSITIVE
// contract (§5).
//
// A well-formed `nsl.module` with:
//
//   - zero `%IDENT%` residue tokens in any `StringAttr` /
//     `FlatSymbolRefAttr` value
//   - zero surviving `nsl.structural_generate` (S10 OK)
//   - zero surviving `nsl.param_int` / `nsl.param_str` (S16 OK)
//   - zero per-scope duplicate `name` decls (S25 OK)
//
// MUST round-trip through the pass with ZERO diagnostics + IR
// preserved verbatim. This is the positive-case test required by
// FR-018 ("MUST not fire on legitimate StringAttr values without
// `%IDENT%` substring") + the SC-006 acceptance gate.

// CHECK-LABEL: nsl.module @CleanBaseline
nsl.module @CleanBaseline {
  // CHECK: nsl.reg "buf_q" : !nsl.bits<8>
  nsl.reg "buf_q" : !nsl.bits<8>
  // CHECK: nsl.wire "src" : !nsl.bits<8>
  %src = nsl.wire "src" : !nsl.bits<8>
  // CHECK: nsl.wire "dst" : !nsl.bits<8>
  %dst = nsl.wire "dst" : !nsl.bits<8>
  // Zero residue, zero generate, zero param, distinct decl names
  // — the pass MUST be a no-op on this input.
  // CHECK: nsl.transfer
  nsl.transfer %dst, %src : !nsl.bits<8>
}

// CHECK-LABEL: nsl.module @AnotherCleanModule
nsl.module @AnotherCleanModule {
  // CHECK: nsl.submodule @inst : @CleanBaseline
  nsl.submodule @inst : @CleanBaseline
}
