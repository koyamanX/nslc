//===- Fmt.h - NSL formatter public umbrella header --------------*- C++ -*-=//
//
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Public umbrella header for `libNslFmt.a` — the NSL formatter library.
// This is the SOLE public header for the `nsl-fmt` tool library
// (Constitution Principle II — single-public-header rule; nsl-fmt is
// NOT one of the named exceptions for `nsl-ast` / `nsl-sema`).
//
// **Public-symbol surface (frozen at exactly 10 — verified by
// `scripts/audit_fmt_api.sh`)**, per
// `specs/010-t2-formatter-v0/contracts/format-api.contract.md` §3:
//
//   Types (3):
//     1. Configuration         — TOML-derived format settings (10 keys)
//     2. LineRange             — `--range LINE:LINE` selector
//     3. FormatResult          — top-level call return shape
//
//   Free functions (7):
//     4. format_buffer         — top-level format entry point
//     5. parse_config_file     — TOML → Configuration
//     6. discover_config       — upward .nsl-fmt.toml walk
//     7. emit_unified_diff     — `--check` diff
//     8. default_configuration — built-in defaults
//     9. config_key_names      — list of 10 known keys
//    10. version_string        — version banner for `nsl-fmt --version`
//
// At T2 Phase 2c the symbols are DECLARED here; their bodies live in
// `lib/Fmt/Format.cpp`, `lib/Fmt/Config.cpp`, `lib/Fmt/Diff.cpp`
// (T076) and similar. Symbols whose full bodies have not yet been
// implemented (parse_config_file, discover_config, emit_unified_diff
// at Phase 2c) carry skeleton implementations that return error
// states with "not yet implemented" diagnostics. Phase 4–6 land the
// real bodies; the public signatures are stable from this commit
// onward.
//
//===----------------------------------------------------------------------===//

#ifndef NSL_FMT_FMT_H
#define NSL_FMT_FMT_H

#include "nsl/Basic/Diagnostic.h"
#include "nsl/Basic/SourceLocation.h"

#include "llvm/ADT/ArrayRef.h"
#include "llvm/ADT/StringRef.h"

#include <optional>
#include <string>
#include <vector>

namespace nsl::fmt {

// -----------------------------------------------------------------------------
// 1. Configuration — TOML-derived format settings (data-model §5)
// -----------------------------------------------------------------------------

struct Configuration {
  enum class Indent          { Spaces2, Spaces4, Tab };
  enum class BraceStyle      { KAndR, Allman };
  enum class TrailingCommas  { Preserve, Add, Remove };
  enum class CommentMode     { All, LeadingOnly, None };

  Indent          indent                       = Indent::Spaces4;
  int             max_line_length              = 100;
  bool            spaces_around_binary_ops     = true;
  bool            spaces_inside_braces         = false;
  bool            align_struct_members         = true;
  bool            align_case_arrows            = true;
  BraceStyle      brace_style                  = BraceStyle::KAndR;
  TrailingCommas  trailing_commas              = TrailingCommas::Preserve;
  int             blank_lines_between_modules  = 2;
  CommentMode     preserve_comments            = CommentMode::All;
};

// -----------------------------------------------------------------------------
// 2. LineRange — `--range LINE:LINE` selector
// -----------------------------------------------------------------------------

struct LineRange {
  int firstLine;   // 1-indexed, inclusive
  int lastLine;    // 1-indexed, inclusive; >= firstLine
};

// -----------------------------------------------------------------------------
// 3. FormatResult — top-level call return shape (data-model §6)
// -----------------------------------------------------------------------------

struct FormatResult {
  enum class Status { Success, Refused, Error };

  Status                              status;
  std::string                         formattedText;   // valid iff status == Success
  std::vector<::nsl::Diagnostic>      diagnostics;     // always populated
};

// -----------------------------------------------------------------------------
// 4. format_buffer — top-level format entry point (FR-017, FR-019)
// -----------------------------------------------------------------------------
//
// Pure function of `(sourceBuffer, config, fileID, range)`. Per
// Principle V byte-stability, the return value is identical across
// builds, hosts, and processes for the same inputs.
//
// Phase 2c skeleton: walks the directive splitter and concatenates
// slice rawText (byte-faithful round-trip — no layout applied yet).
// Phase 3+ wires in the LayoutPlanner so each NSL fragment goes
// through the Wadler-Leijen renderer; Phase 4 wires in `--range`
// support per FR-007.

FormatResult format_buffer(llvm::StringRef        sourceBuffer,
                           const Configuration   &config,
                           ::nsl::FileID          fileID,
                           std::optional<LineRange> range = std::nullopt);

// -----------------------------------------------------------------------------
// 5. parse_config_file — TOML → Configuration (Phase 6 — T103)
// -----------------------------------------------------------------------------
//
// Phase 2c skeleton: returns Status::Error with a "not implemented"
// diagnostic. Phase 6 (T103) wires toml++ for the real parser.

FormatResult parse_config_file(llvm::StringRef tomlBuffer,
                               ::nsl::FileID   fileID);

// -----------------------------------------------------------------------------
// 6. discover_config — upward .nsl-fmt.toml walk (Phase 6 — T106)
// -----------------------------------------------------------------------------
//
// Phase 2c skeleton: always returns std::nullopt. Phase 6 (T106)
// wires `llvm::sys::fs` for the real upward filesystem walk.

std::optional<std::string> discover_config(llvm::StringRef startDir);

// -----------------------------------------------------------------------------
// 7. emit_unified_diff — `--check` diff (Phase 4 — T076)
// -----------------------------------------------------------------------------
//
// Phase 2c skeleton: returns the empty string when oldText ==
// newText, otherwise a single-line "(diff not yet implemented)"
// marker. Phase 4 (T076) wires the Myers-diff implementation.

std::string emit_unified_diff(llvm::StringRef oldText,
                              llvm::StringRef newText,
                              llvm::StringRef oldName,
                              llvm::StringRef newName);

// -----------------------------------------------------------------------------
// 8. default_configuration — built-in defaults (T089)
// -----------------------------------------------------------------------------

Configuration default_configuration() noexcept;

// -----------------------------------------------------------------------------
// 9. config_key_names — list of 10 known TOML keys (T088)
// -----------------------------------------------------------------------------
//
// Returned ArrayRef points at static storage; the strings are
// interned in `lib/Fmt/Config.cpp`. Used by the unknown-key warning
// path (Phase 6 — T104) to suggest "did you mean…?" matches.

llvm::ArrayRef<llvm::StringRef> config_key_names() noexcept;

// -----------------------------------------------------------------------------
// 10. version_string — banner for `nsl-fmt --version` (T087)
// -----------------------------------------------------------------------------

llvm::StringRef version_string() noexcept;

} // namespace nsl::fmt

#endif // NSL_FMT_FMT_H
