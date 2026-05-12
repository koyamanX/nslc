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
#include "LayoutPlanner.h"
#include "LayoutRenderer.h"
#include "nsl/AST/CompilationUnit.h"
#include "nsl/Basic/Diagnostic.h"
#include "nsl/Basic/SourceLocation.h"
#include "nsl/Basic/SourceManager.h"
#include "nsl/Fmt/Fmt.h"
#include "nsl/Lex/Lexer.h"
#include "nsl/Parse/Parser.h"

#include "llvm/ADT/StringRef.h"

#include <algorithm>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace nsl::fmt {

FormatResult format_buffer(llvm::StringRef sourceBuffer,
                           const Configuration &config, ::nsl::FileID fileID,
                           std::optional<LineRange> range) {
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

  // T2 Phase 3-skeleton — per-fragment parse + LayoutPlanner +
  // LayoutRenderer pipeline (research §10 + §11 + §12).
  //
  // Per slice:
  //   * Directive → emit rawText verbatim (FR-012a opaque tokens)
  //   * Whitespace-only NSLFragment → emit rawText verbatim
  //     (parsing is vacuous; would mint an empty CompilationUnit)
  //   * NSLFragment with content → parse as standalone
  //     CompilationUnit; on parse error → ATOMIC REFUSAL of the
  //     whole format_buffer call (FR-012 + Q1 strict refusal).
  //     On success → LayoutPlanner.build(cu) → LayoutRenderer.render().
  //
  // At Phase 3-skeleton the LayoutPlanner uses a verbatim fallback
  // for every AST node kind (emit `Doc::text(<node's source bytes>)`).
  // Total output equals input concatenation — same byte-passthrough
  // as Phase 3-parse. Phase 3-rules incrementally replaces the
  // per-NodeKind handlers with canonical Doc construction (T049-T055).

  // Map Configuration::Indent → LayoutRenderer indent-spaces value.
  // Tab mode uses negative-1 sentinel per LayoutRenderer.h doc.
  int indent_spaces = 4;
  switch (config.indent) {
    case Configuration::Indent::Spaces2: indent_spaces = 2;  break;
    case Configuration::Indent::Spaces4: indent_spaces = 4;  break;
    case Configuration::Indent::Tab:     indent_spaces = -1; break;
  }
  LayoutRenderer renderer;

  std::string out;
  out.reserve(sourceBuffer.size());

  for (const Slice &s : slices) {
    if (s.kind == Slice::Kind::Directive) {
      // FR-012a: opaque pass-through.
      out.append(s.rawText.data(), s.rawText.size());
      continue;
    }

    // NSL fragment. Detect whitespace-only to skip the parse.
    bool only_whitespace = true;
    for (char c : s.rawText) {
      if (c != ' ' && c != '\t' && c != '\n' && c != '\r') {
        only_whitespace = false;
        break;
      }
    }
    if (only_whitespace) {
      out.append(s.rawText.data(), s.rawText.size());
      continue;
    }

    // Parse the fragment as a standalone CompilationUnit.
    ::nsl::SourceManager fragment_sm;
    std::vector<char> bytes(s.rawText.begin(), s.rawText.end());
    ::nsl::FileID fragment_fid =
        fragment_sm.addBufferInMemory("<fragment>", std::move(bytes));
    ::nsl::DiagnosticEngine fragment_diag(fragment_sm);
    ::nsl::Lexer fragment_lex(fragment_sm, fragment_fid, fragment_diag);
    std::unique_ptr<::nsl::ast::CompilationUnit> cu =
        ::nsl::parse::parseCompilationUnit(fragment_lex, fragment_diag);

    if (cu == nullptr || fragment_diag.hasError()) {
      // Atomic refusal — drop everything emitted so far + return.
      result.status = FormatResult::Status::Refused;
      for (const ::nsl::Diagnostic &d : fragment_diag.diagnostics()) {
        result.diagnostics.push_back(d);
      }
      return result;
    }

    // Compute the absolute file line where this NSL fragment starts.
    // The slice's `range.begin().offset()` is a byte offset into the
    // ORIGINAL `sourceBuffer`; count newlines before it (1-indexed).
    int fragmentStartLine = 1;
    {
      std::uint32_t sliceBeginOff = s.range.begin().offset();
      std::uint32_t scanLimit =
          std::min<std::uint32_t>(sliceBeginOff,
                                  static_cast<std::uint32_t>(sourceBuffer.size()));
      for (std::uint32_t i = 0; i < scanLimit; ++i) {
        if (sourceBuffer[i] == '\n') {
          ++fragmentStartLine;
        }
      }
    }

    // Build Doc + render. At Phase 3-skeleton this is byte-identical
    // to s.rawText (verbatim fallback for every AST node kind);
    // canonical-layout overrides fire only on nodes whose line span
    // intersects `range` (T091).
    LayoutPlanner planner(s.rawText, config, range, fragmentStartLine);
    DocPtr doc = planner.build(*cu);
    out.append(renderer.render(doc, config.max_line_length, indent_spaces));
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
