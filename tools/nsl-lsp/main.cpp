// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// tools/nsl-lsp/main.cpp — `nsl-lsp` LSP server entry point (T3).
// Per Constitution Principle II (≤ 70 lines), real work lives in
// `nsl::lsp::runStdioServer` exposed by `lib/LSP/`.

#include "nsl/LSP/Server.h"
#include "nsl/Driver/Version.h"

#include <cstdio>
#include <cstring>

namespace {
constexpr const char *kUsage = "usage: nsl-lsp [--version]\n"
                                "  Speaks LSP over stdin/stdout per "
                                "specs/010-t3-lsp-skeleton/contracts/"
                                "lsp-protocol.contract.md.\n"
                                "  Reads NSL_INCLUDE and "
                                "NSL_LSP_LOG_LEVEL once at startup.\n";
} // namespace

int main(int argc, char **argv) {
  for (int i = 1; i < argc; ++i) {
    const char *a = argv[i];
    if (std::strcmp(a, "--version") == 0 || std::strcmp(a, "-v") == 0) {
      std::printf("nsl-lsp %s\n", NSLC_VERSION_STRING);
      return 0;
    }
    if (std::strcmp(a, "--help") == 0 || std::strcmp(a, "-h") == 0) {
      std::fputs(kUsage, stdout);
      return 0;
    }
  }
  return nsl::lsp::runStdioServer(argc, argv);
}
