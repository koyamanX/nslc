// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// lib/Fmt/ConfigDiscovery.cpp — T2 Phase 6 (T106) implementation of
// `nsl::fmt::discover_config`. Walks upward from `startDir` looking
// for a `.nsl-fmt.toml`; returns the first one found, or nullopt
// if none exists between `startDir` and the filesystem root.
//
// Per `format-api.contract.md` §4 this is the explicit non-pure-
// function carve-out: depends on filesystem state. Callers wanting
// determinism (CI fixtures) bypass via `--config <explicit-path>`.
//
// Spec mapping: FR-013 ("walk upward from each input file looking
// for .nsl-fmt.toml; first one found wins").

#include "nsl/Fmt/Fmt.h"

#include "llvm/ADT/StringRef.h"

#include <filesystem>
#include <optional>
#include <string>
#include <system_error>

namespace nsl::fmt {

std::optional<std::string> discover_config(llvm::StringRef startDir) {
  if (startDir.empty()) {
    return std::nullopt;
  }

  std::error_code ec;
  std::filesystem::path p(startDir.str());

  // Resolve to an absolute path so the upward walk has a stable
  // termination at the filesystem root regardless of how the
  // caller phrased `startDir`.
  std::filesystem::path absolute = std::filesystem::absolute(p, ec);
  if (ec) {
    return std::nullopt;
  }
  p = std::move(absolute);

  // Walk upward. `parent_path()` returns the same path when called
  // on the root, which is our termination signal.
  while (true) {
    std::filesystem::path candidate = p / ".nsl-fmt.toml";
    if (std::filesystem::exists(candidate, ec) && !ec) {
      // exists() returns false on permission errors etc.; treat
      // those as "not present" and keep walking.
      if (std::filesystem::is_regular_file(candidate, ec) && !ec) {
        return candidate.string();
      }
    }
    std::filesystem::path parent = p.parent_path();
    if (parent.empty() || parent == p) {
      break; // reached the root
    }
    p = std::move(parent);
  }
  return std::nullopt;
}

} // namespace nsl::fmt
