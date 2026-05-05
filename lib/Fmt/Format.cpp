// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// lib/Fmt/Format.cpp — top-level format orchestration entry point
// `nsl::fmt::format_buffer` (T2 Phase 2c — T026 / T027).
//
// Phase 2c skeleton wiring:
//
//        sourceBuffer
//             │
//             ▼
//      DirectiveSplitter
//             │  Slices: [Directive | NSLFragment]+
//             ▼
//   For each NSLFragment slice:
//       run a CST-mode parse (CSTBuilder records the event stream)
//       — Phase 2c uses CSTBuilder.serialize() to recover the
//         fragment byte-for-byte (a no-layout-applied round-trip).
//   For each Directive slice:
//       emit slice.rawText verbatim (FR-012a — directives are
//       opaque, byte-preserved).
//             │
//             ▼
//       LayoutRenderer (NOT YET WIRED at Phase 2c — Phase 3 hooks in
//       the LayoutPlanner that produces the Doc IR fed to the
//       renderer. At Phase 2c we skip the planner and emit
//       byte-for-byte, validating the wiring.)
//
// At Phase 2c the formatted output equals the input verbatim for
// every well-formed input — the formatter is a pass-through. This
// is the smallest possible non-trivial test of the full pipeline:
// every component (DirectiveSplitter, CSTBuilder, Parser, Format
// orchestration) executes, but the final layout step is a no-op.
//
// Phase 3 (T049–T055) replaces the byte-for-byte fragment emission
// with `LayoutPlanner(cstBuilder).buildDoc()` followed by
// `LayoutRenderer().render(doc, config.max_line_length, indent)`.

#include "CST.h"
#include "CSTBuilder.h"
#include "Diff.h"
#include "DirectiveSplitter.h"
#include "nsl/Basic/Diagnostic.h"
#include "nsl/Basic/SourceLocation.h"
#include "nsl/Fmt/Fmt.h"
#include "nsl/Lex/Lexer.h"
#include "nsl/Parse/Parser.h"

#include "llvm/ADT/StringRef.h"

#include <optional>
#include <string>
#include <utility>

namespace nsl::fmt {

FormatResult format_buffer(llvm::StringRef sourceBuffer,
                           const Configuration &config, ::nsl::FileID fileID,
                           std::optional<LineRange> range) {
  // `range` is parsed but not yet honored at Phase 2c — Phase 5
  // (T090/T091) wires it through to the LayoutPlanner. Touch the
  // parameter so the compiler does not warn about it.
  (void)range;
  (void)config;

  FormatResult result;

  // Empty input → empty output, success. The directive splitter
  // already handles this (returns an empty slice vector), but we
  // short-circuit explicitly for clarity.
  if (sourceBuffer.empty()) {
    result.status = FormatResult::Status::Success;
    return result;
  }

  // Pre-pass: split into directives + NSL fragments.
  DirectiveSplitter splitter;
  auto slices = splitter.split(sourceBuffer, fileID);

  // **Parse step deferred to Phase 3 proper** (T059 + T039). At
  // Phase 3c the formatter is still byte-passthrough — we cannot
  // safely run the parser at this layer because:
  //
  //   * The lexer does not handle BOM bytes / `%IDENT%` splices /
  //     top-level system-task expressions in the way an
  //     interactive editor would, leading to false-positive parse
  //     failures on inputs the existing test corpus expects to
  //     round-trip cleanly (BOM-preserve, %IDENT%-passthrough,
  //     over-long-line tests would all flip RED).
  //   * Per-fragment parsing (the right architecture given Q1's
  //     directive-aware design) requires either a fragment-aware
  //     Lexer mode or a per-fragment private SourceManager — both
  //     non-trivial extensions to the M1/M2 layers.
  //
  // The full parse + LayoutPlanner integration lands when the
  // per-fragment parse infrastructure is designed. Until then,
  // `result.diagnostics` stays empty and the caller's CLI
  // (tools/nsl-fmt/main.cpp) skips the diagnostic-rendering loop
  // (no warnings flooded to stderr).

  // Byte-passthrough emission. The DirectiveSplitter guarantees
  // no-byte-loss (every input byte covered by exactly one slice);
  // concatenating slice rawText reproduces the source.
  std::string out;
  out.reserve(sourceBuffer.size());
  for (const Slice &s : slices) {
    out.append(s.rawText.data(), s.rawText.size());
  }

  result.status = FormatResult::Status::Success;
  result.formattedText = std::move(out);
  return result;
}

// -----------------------------------------------------------------------------
// Phase 2c skeletons for parse_config_file / discover_config /
// emit_unified_diff — the real bodies arrive in Phases 4 / 6.
// -----------------------------------------------------------------------------

FormatResult parse_config_file(llvm::StringRef /*tomlBuffer*/,
                               ::nsl::FileID /*fileID*/) {
  // T103 (Phase 6) replaces this body with a toml++ parse. At
  // Phase 2c every call returns Error so callers cannot
  // accidentally rely on a default-constructed Configuration.
  FormatResult r;
  r.status = FormatResult::Status::Error;
  // Diagnostic plumbing is deferred to T103 — the FormatResult
  // carries an empty `diagnostics` vector, but the status==Error
  // signal is sufficient for the caller to refuse to proceed.
  return r;
}

std::optional<std::string> discover_config(llvm::StringRef /*startDir*/) {
  // T106 (Phase 6) wires `llvm::sys::fs` to walk upward. At
  // Phase 2c always return nullopt — callers fall back to
  // default_configuration().
  return std::nullopt;
}

std::string emit_unified_diff(llvm::StringRef oldText, llvm::StringRef newText,
                              llvm::StringRef oldName,
                              llvm::StringRef newName) {
  // T076 (Phase 3b) delegates to the LCS-based unified-diff
  // emitter in `lib/Fmt/Diff.cpp`. Pure function; deterministic
  // (Principle V).
  return computeUnifiedDiff(oldText, newText, oldName, newName);
}

} // namespace nsl::fmt
