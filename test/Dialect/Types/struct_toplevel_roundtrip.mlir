// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// RUN: nsl-opt %s | FileCheck %s
// RUN: nsl-opt %s | nsl-opt - | FileCheck %s
//
// Post-merge M4-amendment 2026-05-02 (#3): top-level `nsl.struct`
// (sibling of `nsl.module` under the builtin `mlir::ModuleOp`) is
// legal. NSL grammar places `struct S { ... }` at compilation-unit
// top level (sibling of `module B { ... }` per `lang.ebnf §1`).
//
// This fixture exercises:
//   (1) `nsl.struct @MyRec` parsed/printed/round-tripped at the
//       top level (no enclosing `nsl.module`), and
//   (2) a sibling `nsl.module @M` consuming the struct via
//       `!nsl.struct<@MyRec>` — symbol resolution walks to the
//       enclosing `mlir::ModuleOp` (a SymbolTable) and finds the
//       struct sibling there.
//
// Per Q1 Option A (M4 verifier scope is structural-only) the
// `StructOp::verify()` field-cycle check uses
// `mlir::SymbolTable::getNearestSymbolTable(*this)` — which already
// works for the top-level placement (the builtin `mlir::ModuleOp`
// implements SymbolTable). No verifier-body amendment is required.

// CHECK-LABEL: nsl.struct @MyRec
nsl.struct @MyRec {
  // CHECK: nsl.field_decl "a" : !nsl.bits<8>
  nsl.field_decl "a" : !nsl.bits<8>
  // CHECK: nsl.field_decl "b" : !nsl.bits<24>
  nsl.field_decl "b" : !nsl.bits<24>
}

// CHECK-LABEL: nsl.module @M
nsl.module @M {
  // CHECK: %{{.*}} = nsl.reg "r" : !nsl.struct<@MyRec>
  %r = nsl.reg "r" : !nsl.struct<@MyRec>
}
