// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// tools/nslc/main.cpp — nslc driver entry point (≤60 lines per
// Constitution Principle II). Real work lives in nsl-driver.

#include "nsl/Driver/EmitTokens.h"
#include "nsl/Driver/Version.h"

#include "llvm/ADT/StringRef.h"
#include "llvm/Support/raw_ostream.h"

#include <cstring>

namespace {
constexpr const char *kUsage =
    "usage: nslc [--version] [-I <dir>]... [-D NAME=value]... "
    "[--diagnostic-format=text|json] -emit=<stage> <input>\n";
bool starts(const char *s, const char *p) {
  return std::strncmp(s, p, std::strlen(p)) == 0;
}
} // namespace

int main(int argc, char **argv) {
  nsl::driver::EmitTokensOptions opts;
  llvm::StringRef stage, input;
  for (int i = 1; i < argc; ++i) {
    const char *a = argv[i];
    if (!std::strcmp(a, "--version") || !std::strcmp(a, "-v")) {
      llvm::outs() << "nslc " << NSLC_VERSION_STRING << "\n";
      return 0;
    } else if (starts(a, "-emit=")) {
      stage = a + 6;
    } else if (!std::strcmp(a, "-I") && i + 1 < argc) {
      opts.include_paths.emplace_back(argv[++i]);
    } else if (starts(a, "-I")) {
      opts.include_paths.emplace_back(a + 2);
    } else if (!std::strcmp(a, "-D") && i + 1 < argc) {
      opts.predefined_macros.emplace_back(argv[++i]);
    } else if (starts(a, "-D")) {
      opts.predefined_macros.emplace_back(a + 2);
    } else if (!std::strcmp(a, "--diagnostic-format=json")) {
      opts.diagnostic_json = true;
    } else if (!std::strcmp(a, "--diagnostic-format=text")) {
      opts.diagnostic_json = false;
    } else if (a[0] != '-' && input.empty()) {
      input = a;
    } else {
      llvm::errs() << "unknown argument: " << a << "\n" << kUsage;
      return 2;
    }
  }
  if (stage.empty() || input.empty()) {
    llvm::errs() << "input file required\n" << kUsage;
    return 2;
  }
  if (stage == "tokens")
    return nsl::driver::emitTokens(input, opts, llvm::outs(), llvm::errs());
  llvm::errs() << "unknown emit stage: " << stage << "\n" << kUsage;
  return 2;
}
