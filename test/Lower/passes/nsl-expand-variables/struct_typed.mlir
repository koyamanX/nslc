// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// RUN: nsl-opt -nsl-expand-variables %s | FileCheck %s
//
// M5 US3 / FR-015 — `NSLExpandVariablesPass` (slot 3). Acceptance
// scenario 4 (`spec.md:284`):
//
//   "Given a struct-typed variable variable s : @T; with s.a = x;
//    s.b = y;, when the pass runs, then the post-pass IR has the
//    chain operating per-field (the .a field sees one transfer,
//    the .b field sees one transfer; reads of s materialize via
//    field-level wires)."
//
// **Post-merge M4-amendment 2026-05-02 (#5) status.** The dialect
// now accepts `!nsl.struct<@T>` as a `nsl.variable` result type
// (per the relaxed `NSL_BitsOrStruct` constraint). At THIS commit,
// the pass's scalar-only walk still applies: a struct-typed
// `nsl.variable` with no bit-level uses is erased as a no-op
// expansion (no users → erase). Per-field decomposition (the
// spec's full FR-015 obligation, Acceptance scenario 4) is
// scheduled for a follow-up commit that introduces a struct-pack
// op + a struct-aware visitor pre-pass; until that lands the
// fixture documents the parse-clean post-amendment baseline.

nsl.struct @T {
  nsl.field_decl "a" : !nsl.bits<8>
  nsl.field_decl "b" : !nsl.bits<8>
}

nsl.module @StructTyped {
  // Post-amendment: `nsl.variable` with `!nsl.struct<@T>` parses
  // and verifies. The pass's scalar walk currently treats this
  // op as "no users → erase"; per-field decomposition is the
  // follow-up commit's deliverable.
  %s = nsl.variable "s" : !nsl.struct<@T>
  // CHECK-NOT: nsl.variable
}
