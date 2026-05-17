// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// lib/Fmt/Config.cpp — Configuration record + initial public-API
// stubs (T2 Phase 2c — partial T089 + T088 ahead of schedule, plus
// Phase-6 stubs for parse_config_file / discover_config that
// return error states until T103 / T106 land).
//
// At Phase 2c we ship `default_configuration()` (returns the §5.1
// example values) and `config_key_names()` (returns the 10 known
// keys). The TOML-parsing entry points are skeletons that always
// return `FormatResult::Status::Error` with a "not yet
// implemented at Phase 2c" diagnostic — Phase 6 (T103, T106) wires
// them to toml++.
//
// **Spec / contract anchors**:
//   - data-model.md §5 — Configuration record + 10-key invariant.
//   - format-api.contract.md §3 — public-symbol freeze.
//   - nsl_tooling_design.md §5.1 — example default values.

#include "nsl/Basic/Diagnostic.h"
#include "nsl/Basic/SourceLocation.h"
#include "nsl/Fmt/Fmt.h"

#include "llvm/ADT/ArrayRef.h"
#include "llvm/ADT/StringRef.h"

// T2 Phase 6 (T103) — switch toml++ to noexcept mode (returns
// `parse_result` instead of throwing). Aligns with the project's
// LLVM-style "no exceptions across library boundaries" convention.
#define TOML_EXCEPTIONS 0
#include "toml.hpp"

#include <cstdint>
#include <set>
#include <string>
#include <string_view>

namespace nsl::fmt {

Configuration default_configuration() noexcept {
  // Built-in defaults match the example .nsl-fmt.toml in
  // docs/design/nsl_tooling_design.md §5.1. The Configuration
  // struct's in-class member initialisers already encode these
  // defaults (data-model §5); a default-constructed instance is
  // exactly the right value.
  return Configuration{};
}

llvm::ArrayRef<llvm::StringRef> config_key_names() noexcept {
  // The 10 keys (data-model §5 + format-api contract §3 +
  // nsl_tooling_design.md §5.1). Listed in declaration order to
  // match Configuration's field order so a future "did you mean…?"
  // diagnostic can index into the same vector.
  static const llvm::StringRef kKeys[] = {
      "indent",
      "max_line_length",
      "spaces_around_binary_ops",
      "spaces_inside_braces",
      "align_struct_members",
      "align_case_arrows",
      "brace_style",
      "trailing_commas",
      "blank_lines_between_modules",
      "preserve_comments",
  };
  return llvm::ArrayRef<llvm::StringRef>(kKeys,
                                         sizeof(kKeys) / sizeof(kKeys[0]));
}

llvm::StringRef version_string() noexcept {
  // T087 (Phase 5): the macro is injected by lib/Fmt/CMakeLists.txt
  // via `target_compile_definitions(NslFmt PRIVATE
  // NSL_FMT_VERSION_STRING="nsl-fmt ${NSLC_GIT_DESCRIBE}")`. The
  // git-describe value is shared with nslc's own version banner
  // (cmake/NSLVersion.cmake) — so `nsl-fmt --version` and
  // `nslc --version` always report the same source revision.
  return llvm::StringRef(NSL_FMT_VERSION_STRING);
}

// =============================================================================
// T103 — parse_config_file (Phase 6)
// =============================================================================
//
// Maps a `.nsl-fmt.toml` document onto the Configuration record. The
// 10 known keys are listed in `config_key_names()` (declared above);
// any other key produces a Warning diagnostic but does NOT change
// Status. Out-of-range / wrong-type values for known keys produce
// Error diagnostics + Status::Error.
//
// Frozen diagnostic strings (per `formatting-rules.contract.md` §8):
//   * unknown key:    "warning: unknown configuration key '<k>' at
//   <file>:<line>; ignoring"
//   * out-of-range:   "error: configuration value for '<k>' must be <expected>;
//   got <actual> at <file>:<line>"
//   * TOML parse:     "error: TOML parse error at <file>:<line>:<col>:
//   <description>"

namespace {

// Helper: append a Diagnostic to the result. The fileID is the
// caller-supplied one (usually the .nsl-fmt.toml file's FileID).
void emitDiag(FormatResult &res, ::nsl::Severity sev, ::nsl::FileID fid,
              std::uint32_t offset, std::string msg) {
  ::nsl::Diagnostic d;
  d.severity = sev;
  d.loc = ::nsl::SourceLocation::make(fid, offset);
  d.message = std::move(msg);
  res.diagnostics.push_back(std::move(d));
}

// Helper: render a TOML source position to a per-character offset.
// The toml++ source_position is (line, column) — both 1-indexed.
// We don't have the original buffer here (would be O(N) anyway);
// pack the line into the SourceLocation offset field as an
// approximation. Full file:line:col rendering happens at the
// CLI's stderr renderer once the SourceManager is plumbed through
// (Phase 3 proper).
std::uint32_t packPosition(const toml::source_position &pos) {
  // Cap at SourceLocation::kMaxOffset - 1 to satisfy the
  // SourceLocation::make assert.
  std::uint32_t v = static_cast<std::uint32_t>(pos.line);
  if (v >= ::nsl::SourceLocation::kMaxOffset) {
    v = ::nsl::SourceLocation::kMaxOffset - 1;
  }
  return v;
}

// Set of known keys for unknown-key detection.
const std::set<std::string_view> &knownKeySet() {
  static const std::set<std::string_view> s = {
      "indent",
      "max_line_length",
      "spaces_around_binary_ops",
      "spaces_inside_braces",
      "align_struct_members",
      "align_case_arrows",
      "brace_style",
      "trailing_commas",
      "blank_lines_between_modules",
      "preserve_comments",
  };
  return s;
}

} // namespace

FormatResult parse_config_file(llvm::StringRef tomlBuffer, ::nsl::FileID fileID,
                               Configuration *out) {
  FormatResult res;

  // `out` is required per the contract. Defensive null-check;
  // assert in debug builds.
  if (out == nullptr) {
    res.status = FormatResult::Status::Error;
    emitDiag(
        res, ::nsl::Severity::Error, fileID, 0,
        "internal error: parse_config_file called with null out parameter");
    return res;
  }

  // Start from defaults; overlay parsed values on top.
  *out = default_configuration();

  // toml++ parse (TOML_EXCEPTIONS=0 mode → returns parse_result).
  toml::parse_result parsed =
      toml::parse(std::string_view(tomlBuffer.data(), tomlBuffer.size()),
                  std::string_view{"<config>"});

  if (parsed.failed()) {
    const toml::parse_error &err = parsed.error();
    std::string msg = "TOML parse error: ";
    msg += std::string(err.description());
    res.status = FormatResult::Status::Refused;
    emitDiag(res, ::nsl::Severity::Error, fileID,
             packPosition(err.source().begin), std::move(msg));
    return res;
  }

  const toml::table &tbl = parsed.table();
  bool any_error = false;

  // Lambda to emit an out-of-range diagnostic in the frozen format.
  auto badValue = [&](llvm::StringRef key, const toml::node &n,
                      const char *expected, const std::string &actual) {
    std::string msg = "configuration value for '";
    msg += key.str();
    msg += "' must be ";
    msg += expected;
    msg += "; got ";
    msg += actual;
    emitDiag(res, ::nsl::Severity::Error, fileID,
             packPosition(n.source().begin), std::move(msg));
    any_error = true;
  };

  // ----- indent -----
  if (auto *node = tbl.get("indent")) {
    if (auto sv = node->value<std::string>()) {
      if (*sv == "tab") {
        out->indent = Configuration::Indent::Tab;
      } else if (*sv == "spaces2") {
        out->indent = Configuration::Indent::Spaces2;
      } else if (*sv == "spaces4") {
        out->indent = Configuration::Indent::Spaces4;
      } else {
        badValue("indent", *node, "\"tab\" | \"spaces2\" | \"spaces4\" | 2 | 4",
                 "\"" + *sv + "\"");
      }
    } else if (auto iv = node->value<int64_t>()) {
      if (*iv == 2) {
        out->indent = Configuration::Indent::Spaces2;
      } else if (*iv == 4) {
        out->indent = Configuration::Indent::Spaces4;
      } else {
        badValue("indent", *node, "\"tab\" | \"spaces2\" | \"spaces4\" | 2 | 4",
                 std::to_string(*iv));
      }
    } else {
      badValue("indent", *node,
               "string (\"tab\"|\"spaces2\"|\"spaces4\") or integer (2|4)",
               "(non-string-non-integer)");
    }
  }

  // ----- max_line_length -----
  if (auto *node = tbl.get("max_line_length")) {
    if (auto v = node->value<int64_t>()) {
      if (*v >= 1 && *v <= 10000) {
        out->max_line_length = static_cast<int>(*v);
      } else {
        badValue("max_line_length", *node, "integer in [1, 10000]",
                 std::to_string(*v));
      }
    } else {
      badValue("max_line_length", *node, "integer", "(non-integer)");
    }
  }

  // ----- bool keys -----
  auto readBool = [&](const char *key, bool &slot) {
    if (auto *node = tbl.get(key)) {
      if (auto v = node->value<bool>()) {
        slot = *v;
      } else {
        badValue(key, *node, "boolean", "(non-boolean)");
      }
    }
  };
  readBool("spaces_around_binary_ops", out->spaces_around_binary_ops);
  readBool("spaces_inside_braces", out->spaces_inside_braces);
  readBool("align_struct_members", out->align_struct_members);
  readBool("align_case_arrows", out->align_case_arrows);

  // ----- brace_style -----
  if (auto *node = tbl.get("brace_style")) {
    if (auto sv = node->value<std::string>()) {
      if (*sv == "k&r" || *sv == "kandr") {
        out->brace_style = Configuration::BraceStyle::KAndR;
      } else if (*sv == "allman") {
        out->brace_style = Configuration::BraceStyle::Allman;
      } else {
        badValue("brace_style", *node, "\"k&r\" | \"allman\"",
                 "\"" + *sv + "\"");
      }
    } else {
      badValue("brace_style", *node, "string", "(non-string)");
    }
  }

  // ----- trailing_commas -----
  if (auto *node = tbl.get("trailing_commas")) {
    if (auto sv = node->value<std::string>()) {
      if (*sv == "preserve") {
        out->trailing_commas = Configuration::TrailingCommas::Preserve;
      } else if (*sv == "add") {
        out->trailing_commas = Configuration::TrailingCommas::Add;
      } else if (*sv == "remove") {
        out->trailing_commas = Configuration::TrailingCommas::Remove;
      } else {
        badValue("trailing_commas", *node,
                 "\"preserve\" | \"add\" | \"remove\"", "\"" + *sv + "\"");
      }
    } else {
      badValue("trailing_commas", *node, "string", "(non-string)");
    }
  }

  // ----- blank_lines_between_modules -----
  if (auto *node = tbl.get("blank_lines_between_modules")) {
    if (auto v = node->value<int64_t>()) {
      if (*v >= 0 && *v <= 100) {
        out->blank_lines_between_modules = static_cast<int>(*v);
      } else {
        badValue("blank_lines_between_modules", *node, "integer in [0, 100]",
                 std::to_string(*v));
      }
    } else {
      badValue("blank_lines_between_modules", *node, "integer",
               "(non-integer)");
    }
  }

  // ----- preserve_comments -----
  if (auto *node = tbl.get("preserve_comments")) {
    if (auto sv = node->value<std::string>()) {
      if (*sv == "all") {
        out->preserve_comments = Configuration::CommentMode::All;
      } else if (*sv == "leading_only") {
        out->preserve_comments = Configuration::CommentMode::LeadingOnly;
      } else if (*sv == "none") {
        out->preserve_comments = Configuration::CommentMode::None;
      } else {
        badValue("preserve_comments", *node,
                 "\"all\" | \"leading_only\" | \"none\"", "\"" + *sv + "\"");
      }
    } else {
      badValue("preserve_comments", *node, "string", "(non-string)");
    }
  }

  // ----- unknown-key warnings -----
  // Walk every top-level key and emit a Warning for any key not in
  // the known set. Per FR-015 unknown keys MUST NOT abort the run.
  const auto &known = knownKeySet();
  for (const auto &entry : tbl) {
    std::string_view key(entry.first.str());
    if (known.find(key) == known.end()) {
      std::string msg = "unknown configuration key '";
      msg += std::string(key);
      msg += "'; ignoring";
      emitDiag(res, ::nsl::Severity::Warning, fileID,
               packPosition(entry.second.source().begin), std::move(msg));
    }
  }

  res.status =
      any_error ? FormatResult::Status::Error : FormatResult::Status::Success;
  return res;
}

} // namespace nsl::fmt
