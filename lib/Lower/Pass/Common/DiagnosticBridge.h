// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// lib/Lower/Pass/Common/DiagnosticBridge.h — private RAII helper
// that intercepts MLIR diagnostics and forwards them to the project
// `nsl::DiagnosticEngine` (M5, layer 8a, internal).
//
// **Specification anchors**:
//   - `specs/008-m5-structural-passes/spec.md` FR-019.
//   - `specs/008-m5-structural-passes/data-model.md` §6 — class
//     shape + translation table (Note→Note, Warning→Warning,
//     Error→Error, Remark→Note).
//   - `specs/008-m5-structural-passes/research.md` §12 — bridge
//     rationale; rejected alternatives.
//
// Construction sites: top of `Compilation::lowerToNSL` and top of
// `Compilation::runNSLPasses` (FR-019). RAII: the handler installs
// on construction and un-installs on destruction.
//
// **Internal-only**. Not re-exported from `Lower.h`.

#ifndef NSL_LIB_LOWER_PASS_COMMON_DIAGNOSTICBRIDGE_H
#define NSL_LIB_LOWER_PASS_COMMON_DIAGNOSTICBRIDGE_H

#include "mlir/IR/Diagnostics.h"
#include "mlir/IR/MLIRContext.h"

namespace nsl {
class DiagnosticEngine;
} // namespace nsl

namespace nsl::lower {

/// RAII wrapper around an `mlir::ScopedDiagnosticHandler` that posts
/// every MLIR diagnostic to the supplied `nsl::DiagnosticEngine`.
///
/// Translation rules (per `data-model.md` §6):
///   `mlir::DiagnosticSeverity::Note`    → `nsl::Severity::Note`
///   `mlir::DiagnosticSeverity::Warning` → `nsl::Severity::Warning`
///   `mlir::DiagnosticSeverity::Error`   → `nsl::Severity::Error`
///   `mlir::DiagnosticSeverity::Remark`  → `nsl::Severity::Note`
///     (demoted; the project has no `Remark` tier)
///
/// `mlir::Location` → `nsl::SourceLocation` translation prefers
/// `FileLineColLoc`, falls back to `FusedLoc`'s deepest child.
class DiagnosticBridge {
public:
  DiagnosticBridge(DiagnosticEngine &sink, mlir::MLIRContext &ctx);
  ~DiagnosticBridge();

  DiagnosticBridge(const DiagnosticBridge &) = delete;
  DiagnosticBridge &operator=(const DiagnosticBridge &) = delete;
  DiagnosticBridge(DiagnosticBridge &&) = delete;
  DiagnosticBridge &operator=(DiagnosticBridge &&) = delete;

private:
  DiagnosticEngine &sink_;
  mlir::ScopedDiagnosticHandler handler_;
};

} // namespace nsl::lower

#endif // NSL_LIB_LOWER_PASS_COMMON_DIAGNOSTICBRIDGE_H
