// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// tools/nslc/main.cpp — nslc driver entry point (Principle II target
// ≤62 lines + per-`-emit=*` glue). Real work lives in nsl-driver.

#include "nsl/Driver/EmitAST.h"
#include "nsl/Driver/EmitMLIR.h"
#include "nsl/Driver/EmitTokens.h"
#include "nsl/Driver/Version.h"

#include "llvm/ADT/StringRef.h"
#include "llvm/Support/raw_ostream.h"

#include <cstring>

namespace {
constexpr const char *kUsage =
    "usage: nslc [--version] [-I <dir>]... [-D NAME=value]... "
    "[--diagnostic-format=text|json] -emit=<stage> <input>\n"
    "  -emit=<stage>   Stop after stage. Stages:\n"
    "                    tokens   M1 lex output\n"
    "                    ast      M2/M3 AST snapshot\n"
    "                    mlir     M5 nsl::* MLIR (post-structural-expansion)\n"
    "                    hw       (M6+) — not yet implemented\n"
    "                    verilog  (M7+) — not yet implemented\n";
bool starts(const char *s, const char *p) {
  return std::strncmp(s, p, std::strlen(p)) == 0;
}
} // namespace

int main(int argc, char **argv) {
  nsl::driver::EmitTokensOptions opts;
  llvm::StringRef stage;
  llvm::StringRef input;
  for (int i = 1; i < argc; ++i) {
    const char *a = argv[i];
    if ((std::strcmp(a, "--version") == 0) || (std::strcmp(a, "-v") == 0)) {
      llvm::outs() << "nslc " << NSLC_VERSION_STRING << "\n";
      return 0;
    }
    if (starts(a, "-emit=")) {
      stage = a + 6;
    } else if ((std::strcmp(a, "-I") == 0) && i + 1 < argc) {
      opts.include_paths.emplace_back(argv[++i]);
    } else if (starts(a, "-I") && a[2] != '\0') {
      opts.include_paths.emplace_back(a + 2);
    } else if ((std::strcmp(a, "-D") == 0) && i + 1 < argc) {
      opts.predefined_macros.emplace_back(argv[++i]);
    } else if (starts(a, "-D") && a[2] != '\0') {
      opts.predefined_macros.emplace_back(a + 2);
    } else if (std::strcmp(a, "--diagnostic-format=json") == 0) {
      opts.diagnostic_json = true;
    } else if (std::strcmp(a, "--diagnostic-format=text") == 0) {
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
  if (stage == "tokens") {
    return nsl::driver::emitTokens(input, opts, llvm::outs(), llvm::errs());
  }
  if (stage == "ast") {
    return nsl::driver::emitAST(input, opts, llvm::outs(), llvm::errs());
  }
  if (stage == "mlir") {
    return nsl::driver::emitMLIR(input, opts, llvm::outs(), llvm::errs());
  }
  if (stage == "hw" || stage == "verilog") {
    llvm::errs() << "error: '-emit=" << stage
                 << "' is not yet implemented (planned for "
                 << (stage == "hw" ? "M6" : "M7") << ")\n";
    return 2;
  }
  llvm::errs() << "unknown emit stage: " << stage << "\n" << kUsage;
  return 2;
}
