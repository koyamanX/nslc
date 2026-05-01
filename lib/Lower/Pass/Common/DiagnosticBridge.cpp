// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// lib/Lower/Pass/Common/DiagnosticBridge.cpp
//
// Implementation of the RAII handler that forwards MLIR diagnostics
// to `nsl::DiagnosticEngine` (FR-019). Per `data-model.md` §6.

#include "DiagnosticBridge.h"

#include "nsl/Basic/Diagnostic.h"
#include "nsl/Basic/SourceLocation.h"

#include "mlir/IR/Diagnostics.h"
#include "mlir/IR/Location.h"

namespace nsl::lower {

namespace {

/// Translate `mlir::DiagnosticSeverity` to `nsl::Severity` per the
/// table in `data-model.md` §6.
nsl::Severity translateSeverity(mlir::DiagnosticSeverity s) {
  switch (s) {
  case mlir::DiagnosticSeverity::Note:
    return nsl::Severity::Note;
  case mlir::DiagnosticSeverity::Warning:
    return nsl::Severity::Warning;
  case mlir::DiagnosticSeverity::Error:
    return nsl::Severity::Error;
  case mlir::DiagnosticSeverity::Remark:
    return nsl::Severity::Note; // demoted; project has no Remark tier
  }
  // Unreachable under the current MLIR DiagnosticSeverity enum; default
  // to Error so a future tier addition is loudest, not silent.
  return nsl::Severity::Error;
}

} // namespace

// FOLLOW-UP (post-scaffolding): mlir::Location → nsl::SourceLocation
// translation needs SourceManager-mediated lookup of a FileID by
// filename. The visitor (US1, T047+) and passes (US2/US3/US4) attach
// SourceRange-derived locations to every emitted op, so the
// MLIR-side Location field is always FileLineColLoc-shaped. At
// Phase 2 (this scaffolding) the bridge posts diagnostics with a
// default-constructed SourceLocation; the diagnostic message itself
// still surfaces (so an unexpected MLIR diagnostic at scaffolding
// time is loud), and the location-precision body-fill lands as part
// of the M5 visitor implementation.
DiagnosticBridge::DiagnosticBridge(DiagnosticEngine &sink,
                                   mlir::MLIRContext &ctx)
    : sink_(sink),
      handler_(&ctx, [this](mlir::Diagnostic &d) {
        sink_.report(translateSeverity(d.getSeverity()),
                     nsl::SourceLocation{},
                     d.str());
        // Returning success() consumes the diagnostic so MLIR's
        // built-in printer does not also emit it (we already have).
        return mlir::success();
      }) {}

DiagnosticBridge::~DiagnosticBridge() = default;

} // namespace nsl::lower
