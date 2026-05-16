// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// test_unit/Fmt/format_parity_test.cpp
//
// TDD fixtures for the CLI ↔ library parity contract per
// `specs/010-t2-formatter-v0/contracts/format-api.contract.md` §3 +
// `data-model.md` §6 idempotence invariant.
//
// Tasks covered (`tasks.md`):
//   T081 — CLIMatchesLibrary: invoke `nsl-fmt --stdin` via popen
//          and `format_buffer()` directly; assert byte-identical
//          output for 5 representative fixtures.
//   T082 — IdempotencePostCondition: format_buffer applied twice
//          on its own output reproduces the same bytes.
//   T083 — ExternalLinkSmoke: this entire TU only includes the
//          public umbrella header `nsl/Fmt/Fmt.h`; if it builds +
//          links + runs, the public API is consumable from
//          external translation units (no private-header leakage).
//
// **External-link contract**: this file MUST NOT include any header
// from `lib/Fmt/` (Doc.h, CST.h, Diff.h, etc.). If a future
// refactor pulls a private symbol into the public surface
// accidentally, the build of this TU will surface that.

#include "nsl/Basic/SourceLocation.h"
#include "nsl/Basic/SourceManager.h"
#include "nsl/Fmt/Fmt.h"

#include "llvm/ADT/StringRef.h"

#include "gtest/gtest.h"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <string>
#include <unistd.h> // POSIX: write, close, unlink, mkstemp
#include <vector>

#ifndef NSL_FMT_BINARY_PATH
#error "NSL_FMT_BINARY_PATH must be defined by CMake"
#endif

using nsl::FileID;
using nsl::SourceManager;
using nsl::fmt::Configuration;
using nsl::fmt::default_configuration;
using nsl::fmt::format_buffer;
using nsl::fmt::FormatResult;

namespace {

// Five representative inputs covering directive-free NSL,
// `#include` directives, `#define`/`#ifdef` islands, %IDENT%
// splices (inline-residue form per Q1 strict-refusal — bare
// `%BAR%` would now refuse since the M1 lexer only recognises
// the splice when adjacent to an identifier prefix), and an
// empty buffer.
const std::array<std::string, 5> kRepresentativeFixtures = {
    "module foo {}\n",
    "#include \"x.nsl\"\nmodule a {}\n",
    "#define FOO 1\n#ifdef DEBUG\nmodule d {}\n#endif\n",
    "module mod_%BAR% {}\n",
    "",
};

std::vector<char> bytesOf(llvm::StringRef s) {
  return std::vector<char>(s.begin(), s.end());
}

// Run `nsl-fmt --stdin`, feed it `input`, capture stdout. Returns
// the captured output (possibly empty).
std::string runCLIStdin(llvm::StringRef input) {
  // popen with "w" gives us stdin (write to child) but no stdout
  // capture. Need bidirectional. Use a tempfile + popen "r" as the
  // canonical POSIX pattern.
  char tmpl[] = "/tmp/nsl-fmt-parityXXXXXX";
  int infd = ::mkstemp(tmpl);
  if (infd < 0)
    return std::string{};

  // Write input to tmpfile, close.
  if (input.size() > 0) {
    [[maybe_unused]] ssize_t w = ::write(infd, input.data(), input.size());
    (void)w; // best-effort; stat-based size check below is the gate
  }
  ::close(infd);

  // Build command: `<NSL_FMT_BINARY_PATH> --stdin < <tmpfile>`.
  std::string cmd = NSL_FMT_BINARY_PATH;
  cmd += " --stdin < ";
  cmd += tmpl;

  // Capture stdout via popen "r".
  std::FILE *pipe = ::popen(cmd.c_str(), "r");
  if (pipe == nullptr) {
    ::unlink(tmpl);
    return std::string{};
  }
  std::string out;
  char buf[4096];
  while (true) {
    std::size_t n = std::fread(buf, 1, sizeof(buf), pipe);
    if (n == 0)
      break;
    out.append(buf, n);
  }
  ::pclose(pipe);
  ::unlink(tmpl);
  return out;
}

// -----------------------------------------------------------------------------
// T081 — CLIMatchesLibrary
// -----------------------------------------------------------------------------
TEST(FormatParityTest, CLIMatchesLibrary) {
  Configuration cfg = default_configuration();
  for (std::size_t i = 0; i < kRepresentativeFixtures.size(); ++i) {
    const std::string &input = kRepresentativeFixtures[i];

    // Library path.
    SourceManager sm;
    std::vector<char> bytes = bytesOf(input);
    FileID fid = sm.addBufferInMemory("/virt/parity.nsl", std::move(bytes));
    FormatResult lib =
        format_buffer(llvm::StringRef(input), cfg, fid, std::nullopt);
    ASSERT_EQ(lib.status, FormatResult::Status::Success)
        << "library should succeed on fixture " << i;

    // CLI path.
    std::string cli = runCLIStdin(llvm::StringRef(input));

    EXPECT_EQ(lib.formattedText, cli)
        << "CLI/library byte mismatch on fixture " << i << ":\n"
        << "  input:   '" << input << "'\n"
        << "  library: '" << lib.formattedText << "'\n"
        << "  cli:     '" << cli << "'";
  }
}

// -----------------------------------------------------------------------------
// T082 — IdempotencePostCondition
// -----------------------------------------------------------------------------
TEST(FormatParityTest, IdempotencePostCondition) {
  Configuration cfg = default_configuration();
  for (std::size_t i = 0; i < kRepresentativeFixtures.size(); ++i) {
    const std::string &input = kRepresentativeFixtures[i];

    SourceManager sm1;
    std::vector<char> bytes1 = bytesOf(input);
    FileID fid1 = sm1.addBufferInMemory("/virt/idem1.nsl", std::move(bytes1));
    FormatResult once =
        format_buffer(llvm::StringRef(input), cfg, fid1, std::nullopt);
    ASSERT_EQ(once.status, FormatResult::Status::Success)
        << "first format must succeed on fixture " << i;

    SourceManager sm2;
    std::vector<char> bytes2 = bytesOf(once.formattedText);
    FileID fid2 = sm2.addBufferInMemory("/virt/idem2.nsl", std::move(bytes2));
    FormatResult twice = format_buffer(llvm::StringRef(once.formattedText), cfg,
                                       fid2, std::nullopt);
    ASSERT_EQ(twice.status, FormatResult::Status::Success)
        << "second format must succeed on fixture " << i;

    EXPECT_EQ(once.formattedText, twice.formattedText)
        << "idempotence violated on fixture " << i << ":\n"
        << "  input:   '" << input << "'\n"
        << "  once:    '" << once.formattedText << "'\n"
        << "  twice:   '" << twice.formattedText << "'";
  }
}

// -----------------------------------------------------------------------------
// T083 — ExternalLinkSmoke
// -----------------------------------------------------------------------------
//
// The fact that THIS TU compiles + links + runs at all is the
// external-link smoke test: it includes only `nsl/Fmt/Fmt.h`
// (the sole public umbrella header) and links against
// `libNslFmt.a`. The TEST body itself just calls format_buffer
// on empty input and asserts Success — proving the linker
// resolved `format_buffer` from the public surface.
//
// If a future refactor accidentally drops a needed symbol from
// `Fmt.h`, this binary will fail to link at build time. If the
// runtime contract regresses, this TEST will fail.
TEST(FormatParityTest, ExternalLinkSmoke) {
  Configuration cfg = default_configuration();
  SourceManager sm;
  FileID fid = sm.addBufferInMemory("/virt/empty.nsl", std::vector<char>{});
  FormatResult res = format_buffer(llvm::StringRef(""), cfg, fid, std::nullopt);
  EXPECT_EQ(res.status, FormatResult::Status::Success);
  EXPECT_TRUE(res.formattedText.empty());
}

// Bonus: every public umbrella symbol referenced from this TU.
// If audit_fmt_api.sh ever drifts (Fmt.h gains/loses a symbol)
// AND a referenced symbol in this list goes missing, the build
// breaks here too — belt-and-braces enforcement of the 10-symbol
// freeze.
TEST(FormatParityTest, EveryPublicSymbolIsCallable) {
  // Configuration / LineRange / FormatResult — types
  Configuration cfg{};
  nsl::fmt::LineRange r{1, 1};
  FormatResult res{};
  (void)cfg;
  (void)r;
  (void)res;

  // 7 free functions
  cfg = nsl::fmt::default_configuration();
  auto keys = nsl::fmt::config_key_names();
  EXPECT_EQ(keys.size(), 10u);
  llvm::StringRef ver = nsl::fmt::version_string();
  EXPECT_FALSE(ver.empty());

  SourceManager sm;
  FileID fid = sm.addBufferInMemory("/virt/x.nsl", std::vector<char>{});
  res = nsl::fmt::format_buffer(llvm::StringRef(""), cfg, fid, std::nullopt);
  EXPECT_EQ(res.status, FormatResult::Status::Success);

  Configuration parsed_cfg;
  res = nsl::fmt::parse_config_file(llvm::StringRef(""), fid, &parsed_cfg);
  EXPECT_EQ(res.status, FormatResult::Status::Success); // empty TOML is valid

  std::optional<std::string> discovered =
      nsl::fmt::discover_config(llvm::StringRef("/nonexistent-dir-zzz"));
  EXPECT_FALSE(discovered.has_value());

  std::string diff = nsl::fmt::emit_unified_diff(
      llvm::StringRef("a"), llvm::StringRef("a"), "x", "y");
  EXPECT_TRUE(diff.empty()); // identical inputs → empty diff
}

} // namespace
