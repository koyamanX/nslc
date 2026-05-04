// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// lib/Lower/Pass/Common/DiagnosticBridge.cpp
//
// Implementation of the RAII handler that forwards MLIR diagnostics
// to `nsl::DiagnosticEngine` (FR-019). Per `data-model.md` §6.

#include "DiagnosticBridge.h"

#include "mlir/IR/Diagnostics.h"
#include "mlir/IR/Location.h"
#include "nsl/Basic/Diagnostic.h"
#include "nsl/Basic/SourceLocation.h"

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
    : sink_(sink), handler_(&ctx, [](mlir::Diagnostic & /*d*/) {
        // Phase 3 (US1) forward-progress fix: returning failure()
        // lets MLIR's default printer emit the diagnostic to stderr
        // directly, bypassing nsl::DiagnosticEngine.
        //
        // Rationale: the M1 DiagnosticEngine's text-header writer
        // (lib/Basic/Diagnostic.cpp::writeTextHeaderLine line 54)
        // unconditionally calls sm.resolveVirtual(loc) without an
        // isValid() guard, which aborts via NSL_ABORT on invalid
        // SourceLocation. A future M1-contract amendment (out of
        // scope at M5) should add the isValid() guard so the bridge
        // can route MLIR diagnostics through the project engine
        // properly per FR-019.
        //
        // For now, MLIR diagnostics still surface to stderr (visible
        // to the user), they just don't sort-merge with M1/M2/M3
        // diagnostics in the rendered output.
        return mlir::failure();
      }) {
  (void)sink_; // silence unused-private-field at Phase 3
}

DiagnosticBridge::~DiagnosticBridge() = default;

} // namespace nsl::lower
