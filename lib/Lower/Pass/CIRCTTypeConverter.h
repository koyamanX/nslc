// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// lib/Lower/Pass/CIRCTTypeConverter.h — internal `mlir::TypeConverter`
// subclass for the M6 nsl→CIRCT conversion.
//
// **Specification anchors**:
//   - `specs/010-m6-circt-lowering/data-model.md` §2 — type-mapping
//     table.
//   - `specs/010-m6-circt-lowering/research.md` §2 — registered-
//     conversion shape (no materialization registered for
//     `!nsl.struct` since M5's NSLExpandVariablesPass eliminates
//     all struct-typed SSA values).
//
// PRIVATE: see `NSLToCIRCTPass.h` header notice.

#ifndef NSL_LOWER_PASS_CIRCT_TYPE_CONVERTER_H
#define NSL_LOWER_PASS_CIRCT_TYPE_CONVERTER_H

#include "mlir/IR/BuiltinTypes.h"
#include "mlir/IR/MLIRContext.h"
#include "mlir/Transforms/DialectConversion.h"

namespace nsl::lower {

/// MLIR `TypeConverter` mapping `!nsl.bits<W>` → `iW` and
/// `!nsl.struct<@T>` → packed `iN` per S18 MSB-first ordering.
class CIRCTTypeConverter : public mlir::TypeConverter {
public:
  /// Constructs and registers the two `addConversion` rules.
  /// Per research.md §2: source/target materialization for
  /// `!nsl.bits` ↔ `iW` is a `comb.bitcast` (idiomatic no-op);
  /// no materialization registered for `!nsl.struct` because
  /// M5's NSLExpandVariablesPass guarantees zero `!nsl.struct`
  /// SSA values reach M6 (FR-022 — fail-fast if violated).
  explicit CIRCTTypeConverter(mlir::MLIRContext &ctx);
};

} // namespace nsl::lower

#endif // NSL_LOWER_PASS_CIRCT_TYPE_CONVERTER_H
