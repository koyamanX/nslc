// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// lib/Lower/Lower.cpp — implementation of the public umbrella
// surface declared in `include/nsl/Lower/Lower.h`. Hosts the
// `astToMLIR(...)` thin wrapper and `registerNSLLowerPasses()`
// helper (M5, layer 8a).
//
// **Specification anchors**:
//   - `specs/008-m5-structural-passes/contracts/lower-api.contract.md`
//     §2.1, §2.3.
//   - `specs/008-m5-structural-passes/research.md` §7 — registration
//     helper rationale.
//
// `registerNSLLowerPasses()` is idempotent because the underlying
// `mlir::registerPass(callback)` is idempotent by design (MLIR's
// pass-registry indexes by pass-argument string and silently coexists
// with duplicate registrations).

#include "nsl/Lower/Lower.h"

#include "ASTToMLIR.h"
#include "mlir/Pass/PassRegistry.h"

namespace nsl::lower {

mlir::OwningOpRef<mlir::ModuleOp> astToMLIR(mlir::MLIRContext &ctx,
                                            const ast::CompilationUnit &cu,
                                            const sema::SemaResult &sr) {
  ASTToMLIR visitor(ctx, sr);
  return visitor.lower(cu);
}

void registerNSLLowerPasses() {
  mlir::registerPass([]() { return createNSLResolveParamsPass(); });
  mlir::registerPass([]() { return createNSLExpandGeneratePass(); });
  mlir::registerPass([]() { return createNSLExpandVariablesPass(); });
  mlir::registerPass([]() { return createNSLExplodeSubmodArrayPass(); });
  mlir::registerPass([]() { return createNSLInlineInternalFuncPass(); });
  mlir::registerPass([]() { return createNSLCheckSemanticsPass(); });
}

} // namespace nsl::lower
