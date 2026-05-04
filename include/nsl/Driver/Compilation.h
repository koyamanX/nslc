// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// include/nsl/Driver/Compilation.h — driver-side `Compilation`
// orchestration class (M4 stub; M5 fills in the bodies).
//
// **Specification anchors**:
//   - `specs/007-m4-mlir-dialect/spec.md` FR-004: at M4 the
//     `Compilation` class declared in design §11 gains the
//     `mlirCtx_.loadDialect<nsl::dialect::NSLDialect>()` call site
//     AND `lowerToNSL` / `runNSLPasses` member-function declarations
//     whose **bodies** are M5 deliverables. At M4 the bodies are
//     trivial diagnostic stubs that emit "MLIR lowering not yet
//     implemented; see M5".
//   - `docs/design/nsl_compiler_design.md` §11 lines 1120–1175 —
//     full design-doc sketch of the class. **At M4 only the dialect-
//     loading bits and the two stub-bearing member functions are
//     instantiated**; the full ctor option struct, the `run()`
//     method, and the per-stage pipeline land at M5+.
//   - `specs/007-m4-mlir-dialect/data-model.md` §5 — driver dialect-
//     load surface entity catalog.
//
// `Compilation` is a developer/test scaffold at M4: the public
// `nslc` CLI does not reach `lowerToNSL` / `runNSLPasses` (FR-023
// rejects `-emit=mlir`/`-emit=hw`/`-emit=verilog`). The stubs exist
// so the binary links cleanly and so the M5 implementer can fill
// them in without re-introducing the `Compilation` plumbing.

#ifndef NSL_DRIVER_COMPILATION_H
#define NSL_DRIVER_COMPILATION_H

#include "mlir/IR/BuiltinOps.h"
#include "mlir/IR/MLIRContext.h"
#include "mlir/IR/OwningOpRef.h"
#include "mlir/Support/LogicalResult.h"

namespace nsl {
class DiagnosticEngine;
} // namespace nsl

namespace nsl::ast {
class CompilationUnit;
} // namespace nsl::ast

namespace nsl::sema {
struct SemaResult;
} // namespace nsl::sema

namespace nsl::driver {

/// Pipeline orchestration owned by the M4+ driver. At M4 this is a
/// stub-bearing scaffold (see FR-004); the full per-stage pipeline
/// lands at M5+.
///
/// The constructor loads the `nsl` dialect into the embedded
/// `mlir::MLIRContext` per design §11 line 1145, fulfilling FR-004's
/// "dialect is registered against the driver-side context" rule even
/// though the public `nslc` CLI does not exercise the MLIR path at M4.
class Compilation {
public:
  /// Construct with a diagnostic engine; loads the `nsl` dialect into
  /// the embedded `mlir::MLIRContext`. The `DiagnosticEngine` is held
  /// by reference so all downstream stages (`lowerToNSL`,
  /// `runNSLPasses`, M5+ `lowerToCIRCT` / `emit`) route diagnostics
  /// through the same sink.
  explicit Compilation(DiagnosticEngine &diag);

  ~Compilation();

  Compilation(const Compilation &) = delete;
  Compilation &operator=(const Compilation &) = delete;

  /// Lower an AST CompilationUnit + Sema result into the `nsl` MLIR
  /// dialect (M5 deliverable; M4 stub).
  ///
  /// **At M4**: emits `Severity::Fatal` "MLIR lowering not yet
  /// implemented; see M5" through the bound `DiagnosticEngine` and
  /// returns an empty `OwningOpRef`. Reachable only via direct C++
  /// caller; the `nslc` CLI does not invoke this at M4 (FR-023
  /// rejects `-emit=mlir`).
  mlir::OwningOpRef<mlir::ModuleOp> lowerToNSL(ast::CompilationUnit &unit,
                                               sema::SemaResult &sema_result);

  /// Run the structural-expansion passes over the `nsl` dialect IR
  /// (M5 deliverable; M4 stub).
  ///
  /// **At M4**: parallel stub. Emits the same diagnostic and returns
  /// `mlir::failure()`.
  mlir::LogicalResult runNSLPasses(mlir::ModuleOp module);

  /// Read-only accessor for the embedded MLIR context. M5 lowering
  /// passes consume this when constructing ops.
  mlir::MLIRContext &context() noexcept { return mlir_ctx_; }

private:
  DiagnosticEngine &diag_;
  mlir::MLIRContext mlir_ctx_;
};

} // namespace nsl::driver

#endif // NSL_DRIVER_COMPILATION_H
