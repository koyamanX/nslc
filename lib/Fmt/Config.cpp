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

#include "nsl/Fmt/Fmt.h"

#include "llvm/ADT/ArrayRef.h"
#include "llvm/ADT/StringRef.h"

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
  return llvm::ArrayRef<llvm::StringRef>(kKeys, sizeof(kKeys) / sizeof(kKeys[0]));
}

llvm::StringRef version_string() noexcept {
  // T087 Phase 2c stub. Phase 5 (US3) will replace with a string
  // built from CMake-defined NSL_PROJECT_VERSION + LLVM_PROJECT_VERSION.
  return llvm::StringRef("nsl-fmt version 0.0.0 (T2 Phase 2c)");
}

} // namespace nsl::fmt
