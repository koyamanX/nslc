// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// lib/Lower/Pass/CIRCTTypeConverter.cpp — implementation of
// `CIRCTTypeConverter` (M6).

#include "CIRCTTypeConverter.h"

#include "mlir/IR/BuiltinTypes.h"
#include "nsl/Dialect/NSL/IR/NSLDialect.h"

namespace nsl::lower {

CIRCTTypeConverter::CIRCTTypeConverter(mlir::MLIRContext &ctx) {
  // !nsl.bits<W> → iW. Per data-model.md §2 the only width-preserving
  // conversion needed; M5's NSLExpandVariablesPass eliminates every
  // !nsl.struct-typed SSA value before M6 sees the IR (FR-022), and
  // !nsl.mem-typed values are referenced via `nsl.mem` declarations
  // whose lowering is handled directly by StatePatterns (Phase 6
  // T110), so the type converter does not need a rule for them.
  addConversion([](mlir::Type type) -> mlir::Type {
    if (auto bits = mlir::dyn_cast<nsl::dialect::BitsType>(type)) {
      return mlir::IntegerType::get(type.getContext(), bits.getWidth());
    }
    return type; // identity for built-in types (IntegerType, etc.)
  });

  (void)ctx; // Reserved — future struct-type conversion may need
             // ctx for sibling-symbol resolution.
}

} // namespace nsl::lower
