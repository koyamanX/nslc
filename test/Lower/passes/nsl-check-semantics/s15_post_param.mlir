// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// XFAIL: *
// RUN: nsl-opt --verify-diagnostics --split-input-file -nsl-check-semantics %s
//
// M5 US4 / FR-018 — sensitive-`Sn` re-check S15 per
// `pass-pipeline.contract.md` §3 row S15:
//
//   "S15 | bit-slice index resolves to non-compile-time-constant
//    after slot 1 | error: bit-slice index is non-constant after
//    parameter resolution"
//
// **DEFERRED at M5 (vacuous on frozen 79-op surface)** — at M5's
// frozen dialect surface, `nsl.slice` carries `I64Attr` lo/hi
// indices (not operand-side SSA values referencing `nsl.constant`
// or unresolved param refs). The "non-constant after slot 1"
// condition is therefore unreachable on PURE-NSL inputs at M5;
// the re-check helper is a documented no-op stub.
//
// When a future M4+ amendment introduces a bit-slice op variant
// whose index slot is operand-side `Value` (rather than `I64Attr`),
// this re-check helper becomes meaningful: walk every
// `nsl.slice` op and assert each index `Value` is defined by an
// `nsl.constant`. Until then, this fixture is XFAIL'd.

nsl.module @S15PostParam {
  // expected-error@+1 {{bit-slice index is non-constant after parameter resolution}}
  nsl.wire "src" : !nsl.bits<32>
}
