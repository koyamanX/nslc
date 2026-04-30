// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// lib/Dialect/NSL/IR/NSLTypes.td — TableGen records for the three
// `!nsl.*` dialect types (M4, layer 7).
//
// **Specification anchors**:
//   - `specs/007-m4-mlir-dialect/spec.md` FR-007 / FR-008: three
//     dialect types with default printer/parser symmetric round-trip.
//   - `specs/007-m4-mlir-dialect/data-model.md` §3 — !nsl.bits<N>,
//     !nsl.struct<@T>, !nsl.mem<[D x T]>.
//   - `specs/007-m4-mlir-dialect/research.md` §5 — interning via
//     MLIR's standard `TypeStorage`.

#ifndef NSL_TYPES_TD
#define NSL_TYPES_TD

include "mlir/IR/AttrTypeBase.td"
include "NSLDialect.td"

//===----------------------------------------------------------------------===//
// !nsl.bits<N> — N-wide bit vector (lowers bijectively to CIRCT's `iN`).
//===----------------------------------------------------------------------===//

def NSL_BitsType : NSL_Type<"Bits", "bits"> {
  let summary = "N-wide bit-vector type";
  let description = [{
    `!nsl.bits<N>` is an N-wide bit vector (the NSL equivalent of an
    unsigned integer of width N). At M6 the lowering is bijective:
    `!nsl.bits<N>` → CIRCT `i<N>`. Zero-width (`!nsl.bits<0>`) is a
    degenerate but valid form per spec Edge-Cases.
  }];
  let parameters = (ins "unsigned":$width);
  let assemblyFormat = "`<` $width `>`";
}

//===----------------------------------------------------------------------===//
// !nsl.struct<@StructName> — structural type referring to a sibling
// `nsl.struct` op via a SymbolRefAttr.
//===----------------------------------------------------------------------===//

def NSL_StructType : NSL_Type<"Struct", "struct"> {
  let summary = "Structural type referring to a sibling `nsl.struct` op";
  let description = [{
    `!nsl.struct<@T>` is a type-level reference to a sibling
    `nsl.struct` declaration. Lowers to `hw.struct<...>` at M6 via
    expansion of the referenced `nsl.struct` body.
  }];
  let parameters = (ins "::mlir::SymbolRefAttr":$name);
  let assemblyFormat = "`<` $name `>`";
}

//===----------------------------------------------------------------------===//
// !nsl.mem<[D x T]> — depth-D memory of element type T (lowers to
// `hw.array<D x T>` at M6).
//===----------------------------------------------------------------------===//

def NSL_MemType : NSL_Type<"Mem", "mem"> {
  let summary = "Memory type with integer depth and element type";
  let description = [{
    `!nsl.mem<[D x T]>` is a memory of `D` elements of type `T`. Per
    spec Edge-Cases, depth-zero is a degenerate but valid form.
  }];
  let parameters = (ins "unsigned":$depth, "::mlir::Type":$elementType);
  let assemblyFormat = "`<` `[` $depth `x` $elementType `]` `>`";
}

#endif // NSL_TYPES_TD
