// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// lib/Dialect/NSL/IR/NSLDialect.td — TableGen declaration of the
// `nsl` MLIR dialect (M4, layer 7).
//
// **Specification anchors**:
//   - `specs/007-m4-mlir-dialect/spec.md` FR-006: dialect named "nsl"
//     with `cppNamespace = "::nsl::dialect"` and
//     `useDefaultTypePrinterParser = 1`.
//   - `docs/design/nsl_compiler_design.md` §7 lines 943–947 — the
//     dialect class shape.
//   - `specs/007-m4-mlir-dialect/data-model.md` §1 — entity catalog.
//   - `specs/007-m4-mlir-dialect/research.md` §3 — three-file
//     partition (NSLDialect.td + NSLOps.td + NSLTypes.td).
//
// At Phase 2 this file declares the dialect class and the `NSL_Op`
// base class only; ops + types are added by Phase 3 (US1) per the
// TDD ordering.

#ifndef NSL_DIALECT_TD
#define NSL_DIALECT_TD

include "mlir/IR/OpBase.td"

//===----------------------------------------------------------------------===//
// Dialect definition
//===----------------------------------------------------------------------===//

def NSL_Dialect : Dialect {
  let name = "nsl";
  let cppNamespace = "::nsl::dialect";
  let summary = "The NSL hardware-description IR dialect.";
  let description = [{
    The `nsl` dialect carries the post-Sema, pre-CIRCT IR for the
    NSL hardware-description language. Its op set mirrors the NSL
    grammar's structural categories (modules, storage, control
    terminals, action blocks, atomic actions, procedures, system
    tasks) plus a small set of marker / lowering-helper ops that
    bridge to CIRCT during M5/M6 lowering. See
    `docs/design/nsl_compiler_design.md` §§7–10.
  }];

  // Phase 2: 0 (no types yet — Phase 3 T070 flips back to 1 when the
  // 3 type records land in NSLTypes.td). With useDefaultTypePrinterParser=1
  // and zero type records, TableGen still declares NSLDialect::parseType /
  // ::printType in the dialect-class header but emits no definitions
  // (defs go in NSLTypes.cpp.inc which has no real content at Phase 2),
  // producing undefined-reference linker errors at any consumer that
  // pulls libnsl-dialect.a. Phase 3 T070 amends this to 1.
  let useDefaultTypePrinterParser = 0;
}

//===----------------------------------------------------------------------===//
// Base classes
//===----------------------------------------------------------------------===//

// Base class every `nsl.*` op inherits from. Provides the dialect
// link and a uniform mnemonic format ("nsl.<mnemonic>") via the
// upstream `Op<>` machinery.
class NSL_Op<string mnemonic, list<Trait> traits = []>
    : Op<NSL_Dialect, mnemonic, traits>;

// Base class every `!nsl.*` type inherits from. The Phase 2 file
// is empty of real type records; populated in Phase 3 / US1.
class NSL_Type<string name, string typeMnemonic, list<Trait> traits = []>
    : TypeDef<NSL_Dialect, name, traits> {
  let mnemonic = typeMnemonic;
}

#endif // NSL_DIALECT_TD
