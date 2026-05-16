// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// test_unit/Fmt/config_parser_test.cpp
//
// TDD fixtures for `nsl::fmt::parse_config_file` + `default_configuration`
// + `config_key_names`. Per `specs/010-t2-formatter-v0/data-model.md` §5
// + `contracts/format-api.contract.md` §4.
//
// Tasks covered (`tasks.md`):
//   T094 — DefaultsMatchSpec: `default_configuration()` returns
//          the §5.1 example values for every field.
//   T095 — ParseAllTenKeys: a TOML file with all 10 keys set non-default
//          round-trips through `parse_config_file()` correctly.
//   T096 — UnknownKeyWarning: unknown TOML key produces a Warning
//          diagnostic + Status::Success (NOT Error) per FR-015.
//   T097 — OutOfRangeError: `indent = "potato"` produces Status::Error
//          + the frozen "must be" diagnostic string.

#include "nsl/Basic/SourceLocation.h"
#include "nsl/Basic/SourceManager.h"
#include "nsl/Fmt/Fmt.h"

#include "llvm/ADT/StringRef.h"

#include "gtest/gtest.h"
#include <algorithm>
#include <string>
#include <vector>

using nsl::FileID;
using nsl::SourceManager;
using nsl::fmt::config_key_names;
using nsl::fmt::Configuration;
using nsl::fmt::default_configuration;
using nsl::fmt::FormatResult;
using nsl::fmt::parse_config_file;

namespace {

std::vector<char> bytesOf(const char *literal) {
  std::vector<char> out;
  while (*literal != 0)
    out.push_back(*literal++);
  return out;
}

FileID addBufferOf(SourceManager &sm, llvm::StringRef name,
                   llvm::StringRef content) {
  std::vector<char> bytes(content.begin(), content.end());
  return sm.addBufferInMemory(name.str(), std::move(bytes));
}

// -----------------------------------------------------------------------------
// T094 — DefaultsMatchSpec
// -----------------------------------------------------------------------------
TEST(ConfigParserTest, DefaultsMatchSpec) {
  Configuration c = default_configuration();
  EXPECT_EQ(c.indent, Configuration::Indent::Spaces4);
  EXPECT_EQ(c.max_line_length, 100);
  EXPECT_TRUE(c.spaces_around_binary_ops);
  EXPECT_FALSE(c.spaces_inside_braces);
  EXPECT_TRUE(c.align_struct_members);
  EXPECT_TRUE(c.align_case_arrows);
  EXPECT_EQ(c.brace_style, Configuration::BraceStyle::KAndR);
  EXPECT_EQ(c.trailing_commas, Configuration::TrailingCommas::Preserve);
  EXPECT_EQ(c.blank_lines_between_modules, 2);
  EXPECT_EQ(c.preserve_comments, Configuration::CommentMode::All);
}

TEST(ConfigParserTest, KeyNamesAreTen) {
  auto keys = config_key_names();
  EXPECT_EQ(keys.size(), 10u);
  // Spot-check a few names by content.
  std::vector<std::string> as_strings;
  for (auto k : keys)
    as_strings.emplace_back(k.str());
  EXPECT_NE(std::find(as_strings.begin(), as_strings.end(), "indent"),
            as_strings.end());
  EXPECT_NE(
      std::find(as_strings.begin(), as_strings.end(), "preserve_comments"),
      as_strings.end());
  EXPECT_NE(std::find(as_strings.begin(), as_strings.end(), "brace_style"),
            as_strings.end());
}

// -----------------------------------------------------------------------------
// T095 — ParseAllTenKeys
// -----------------------------------------------------------------------------
TEST(ConfigParserTest, ParseAllTenKeys) {
  // A TOML file flipping every key to a non-default value.
  const char *kTOML = "indent = \"tab\"\n"
                      "max_line_length = 80\n"
                      "spaces_around_binary_ops = false\n"
                      "spaces_inside_braces = true\n"
                      "align_struct_members = false\n"
                      "align_case_arrows = false\n"
                      "brace_style = \"allman\"\n"
                      "trailing_commas = \"add\"\n"
                      "blank_lines_between_modules = 1\n"
                      "preserve_comments = \"leading_only\"\n";

  SourceManager sm;
  FileID fid = addBufferOf(sm, "/virt/test.toml", kTOML);
  Configuration cfg;
  FormatResult res = parse_config_file(llvm::StringRef(kTOML), fid, &cfg);

  ASSERT_EQ(res.status, FormatResult::Status::Success)
      << "parse should succeed; diagnostics:";
  for (auto &d : res.diagnostics) {
    ADD_FAILURE() << "  unexpected diagnostic: " << d.message;
  }

  EXPECT_EQ(cfg.indent, Configuration::Indent::Tab);
  EXPECT_EQ(cfg.max_line_length, 80);
  EXPECT_FALSE(cfg.spaces_around_binary_ops);
  EXPECT_TRUE(cfg.spaces_inside_braces);
  EXPECT_FALSE(cfg.align_struct_members);
  EXPECT_FALSE(cfg.align_case_arrows);
  EXPECT_EQ(cfg.brace_style, Configuration::BraceStyle::Allman);
  EXPECT_EQ(cfg.trailing_commas, Configuration::TrailingCommas::Add);
  EXPECT_EQ(cfg.blank_lines_between_modules, 1);
  EXPECT_EQ(cfg.preserve_comments, Configuration::CommentMode::LeadingOnly);
}

// -----------------------------------------------------------------------------
// T096 — UnknownKeyWarning
// -----------------------------------------------------------------------------
TEST(ConfigParserTest, UnknownKeyWarning) {
  // Unknown keys MUST produce a Warning diagnostic but MUST NOT
  // fail the parse (FR-015).
  const char *kTOML = "indent = 4\n"
                      "totally_made_up_key = \"hello\"\n"
                      "another_unknown = 42\n";

  SourceManager sm;
  FileID fid = addBufferOf(sm, "/virt/unknown.toml", kTOML);
  Configuration cfg;
  FormatResult res = parse_config_file(llvm::StringRef(kTOML), fid, &cfg);

  EXPECT_EQ(res.status, FormatResult::Status::Success)
      << "unknown keys must NOT fail the parse (FR-015)";
  EXPECT_EQ(cfg.indent, Configuration::Indent::Spaces4)
      << "valid key still applies";

  // Expect at least 2 Warning diagnostics — one per unknown key.
  int warnings = 0;
  for (auto &d : res.diagnostics) {
    if (d.severity == nsl::Severity::Warning) {
      ++warnings;
      EXPECT_NE(d.message.find("unknown configuration key"), std::string::npos)
          << "warning text should mention 'unknown configuration key'; got: "
          << d.message;
    }
  }
  EXPECT_EQ(warnings, 2) << "expected 2 unknown-key warnings; got " << warnings;
}

// -----------------------------------------------------------------------------
// T097 — OutOfRangeError
// -----------------------------------------------------------------------------
TEST(ConfigParserTest, OutOfRangeError) {
  // `indent = "potato"` is a known key with an unknown enum
  // value → MUST produce Status::Error + a frozen "must be"
  // diagnostic (FR-016).
  const char *kTOML = "indent = \"potato\"\n";

  SourceManager sm;
  FileID fid = addBufferOf(sm, "/virt/bad.toml", kTOML);
  Configuration cfg;
  FormatResult res = parse_config_file(llvm::StringRef(kTOML), fid, &cfg);

  EXPECT_EQ(res.status, FormatResult::Status::Error);

  bool found = false;
  for (auto &d : res.diagnostics) {
    if (d.severity == nsl::Severity::Error &&
        d.message.find("must be") != std::string::npos &&
        d.message.find("indent") != std::string::npos) {
      found = true;
    }
  }
  EXPECT_TRUE(found)
      << "expected an Error diagnostic mentioning 'indent' and 'must be'";
}

TEST(ConfigParserTest, TomlSyntaxErrorIsRefused) {
  // Malformed TOML → Status::Refused + an Error diagnostic.
  const char *kTOML = "indent = [ unclosed\n";

  SourceManager sm;
  FileID fid = addBufferOf(sm, "/virt/syntax.toml", kTOML);
  Configuration cfg;
  FormatResult res = parse_config_file(llvm::StringRef(kTOML), fid, &cfg);

  EXPECT_EQ(res.status, FormatResult::Status::Refused);
  EXPECT_FALSE(res.diagnostics.empty());
}

// -----------------------------------------------------------------------------
// Bonus — out parameter null check
// -----------------------------------------------------------------------------
TEST(ConfigParserTest, NullOutReturnsError) {
  SourceManager sm;
  FileID fid = addBufferOf(sm, "/virt/x.toml", "");
  FormatResult res = parse_config_file(llvm::StringRef(""), fid, nullptr);
  EXPECT_EQ(res.status, FormatResult::Status::Error);
}

} // namespace
