// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// lib/Fmt/Format.cpp — top-level format orchestration entry point
// `nsl::fmt::format_buffer` (T2 Phase 3-parse — T026/T027/T059/T061).
//
// Pipeline:
//
//        sourceBuffer
//             │
//             ▼
//      DirectiveSplitter
//             │  Slices: [Directive | NSLFragment]+
//             ▼
//   For each NSLFragment slice (skip whitespace-only):
//       construct private SourceManager + Lexer + DiagnosticEngine
//       parseCompilationUnit; on parse error → ATOMIC REFUSAL
//       (no partial output) per spec FR-012 + Session 2026-05-05 Q1.
//   For each Directive slice: opaque pass-through (FR-012a).
//             │
//             ▼
//      Byte-passthrough emission (Phase 3-parse): emit slice.rawText
//      verbatim. Phase 3+ replaces this with a LayoutPlanner walk
//      over the parsed CompilationUnit that produces a Doc tree
//      → LayoutRenderer.
//             │
//             ▼
//      Post-pass: CRLF → LF normalization (T061);
//                 trailing-newline normalization (R7 per Q3 — output
//                 always ends with exactly one `\n` when non-empty).
//             │
//             ▼
//        formattedText
//
// Documented limitation (research §10): NSL fragments split mid-
// body by `#ifdef`/`#endif` are NOT well-formed standalone
// CompilationUnits and WILL refuse. Same trade-off as
// `clang-format`'s preprocessor model.

#include "CST.h"
#include "CSTBuilder.h"
#include "Diff.h"
#include "DirectiveSplitter.h"
#include "nsl/AST/CompilationUnit.h"
#include "nsl/Basic/Diagnostic.h"
#include "nsl/Basic/SourceLocation.h"
#include "nsl/Basic/SourceManager.h"
#include "nsl/Fmt/Fmt.h"
#include "nsl/Lex/Lexer.h"
#include "nsl/Parse/Parser.h"

#include "llvm/ADT/StringRef.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

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

  // T2 Phase 3-parse (T059) — per-fragment parse with atomic
  // refusal per research §10.
  //
  // Each NSLFragment slice is parsed as a standalone CompilationUnit
  // through a fresh SourceManager + Lexer + DiagnosticEngine. If
  // ANY fragment fails to lex+parse, the WHOLE format_buffer call
  // returns Status::Refused — no partial output (FR-012 atomic
  // refusal as clarified Session 2026-05-05 — Q1).
  //
  // Documented limitation (research §10): NSL fragments split mid-
  // body by `#ifdef`/`#endif` are NOT well-formed standalone
  // CompilationUnits and WILL refuse. Same trade-off as
  // `clang-format`'s preprocessor model.
  //
  // Phase 3+ replaces the byte-passthrough emission below with
  // `LayoutPlanner(cu).buildDoc()` + LayoutRenderer. For now,
  // successful parse → emit slice.rawText verbatim (idempotent by
  // construction).
  for (const Slice &s : slices) {
    if (s.kind != Slice::Kind::NSLFragment) {
      continue; // Directives are opaque; no parse needed.
    }
    // Skip whitespace-only fragments (parsing them is vacuous and
    // wastes a SourceManager construction).
    bool only_whitespace = true;
    for (char c : s.rawText) {
      if (c != ' ' && c != '\t' && c != '\n' && c != '\r') {
        only_whitespace = false;
        break;
      }
    }
    if (only_whitespace) {
      continue;
    }

    ::nsl::SourceManager fragment_sm;
    std::vector<char> bytes(s.rawText.begin(), s.rawText.end());
    ::nsl::FileID fragment_fid =
        fragment_sm.addBufferInMemory("<fragment>", std::move(bytes));
    ::nsl::DiagnosticEngine fragment_diag(fragment_sm);
    ::nsl::Lexer fragment_lex(fragment_sm, fragment_fid, fragment_diag);
    std::unique_ptr<::nsl::ast::CompilationUnit> cu =
        ::nsl::parse::parseCompilationUnit(fragment_lex, fragment_diag);

    if (cu == nullptr || fragment_diag.hasError()) {
      // Atomic refusal: copy diagnostics + return Refused. SourceLocation
      // values reference the per-fragment private FileID (caller's
      // CLI surfaces the message text; full file:line:col mapping
      // is a Phase 3+ refinement).
      result.status = FormatResult::Status::Refused;
      for (const ::nsl::Diagnostic &d : fragment_diag.diagnostics()) {
        result.diagnostics.push_back(d);
      }
      return result;
    }
    // Parse succeeded — fragment is well-formed. Phase 3+ will
    // layout it via the LayoutPlanner; for now just continue to
    // verbatim emission below.
  }

  // Byte-passthrough emission (still — the LayoutPlanner replaces
  // this in Phase 3+). The DirectiveSplitter guarantees no-byte-
  // loss; concatenating slice rawText reproduces the source.
  std::string out;
  out.reserve(sourceBuffer.size());
  for (const Slice &s : slices) {
    out.append(s.rawText.data(), s.rawText.size());
  }

  // T061 — CRLF → LF normalization (per spec edge case + R7
  // adjacent rule). Applied to the FINAL assembled output.
  {
    std::string normalized;
    normalized.reserve(out.size());
    for (std::size_t i = 0; i < out.size(); ++i) {
      if (out[i] == '\r' && i + 1 < out.size() && out[i + 1] == '\n') {
        normalized.push_back('\n');
        ++i;
      } else if (out[i] == '\r') {
        normalized.push_back('\n');
      } else {
        normalized.push_back(out[i]);
      }
    }
    out = std::move(normalized);
  }

  // R7 — trailing-newline normalization (Session 2026-05-05 — Q3):
  // non-empty output ALWAYS ends with exactly one `\n`. Multiple
  // trailing `\n` collapse to one; missing trailing `\n` is added.
  // Empty input → empty output (no spurious `\n`).
  if (!out.empty()) {
    while (out.size() > 1 && out.back() == '\n' && out[out.size() - 2] == '\n') {
      out.pop_back();
    }
    if (out.back() != '\n') {
      out.push_back('\n');
    }
  }

  result.status = FormatResult::Status::Success;
  result.formattedText = std::move(out);
  return result;
}

// -----------------------------------------------------------------------------
// Phase 2c skeletons for parse_config_file / discover_config /
// emit_unified_diff — the real bodies arrive in Phases 4 / 6.
// -----------------------------------------------------------------------------

// `parse_config_file` is implemented in lib/Fmt/Config.cpp (T103);
// `discover_config` lives in lib/Fmt/ConfigDiscovery.cpp (T106).

std::string emit_unified_diff(llvm::StringRef oldText, llvm::StringRef newText,
                              llvm::StringRef oldName,
                              llvm::StringRef newName) {
  // T076 (Phase 3b) delegates to the LCS-based unified-diff
  // emitter in `lib/Fmt/Diff.cpp`. Pure function; deterministic
  // (Principle V).
  return computeUnifiedDiff(oldText, newText, oldName, newName);
}

} // namespace nsl::fmt
