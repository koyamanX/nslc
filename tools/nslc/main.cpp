// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// tools/nslc/main.cpp — nslc driver entry point.
//
// Per Constitution Principle II this file MUST stay ≤60 lines and
// delegate behavior to nsl-driver. At M0 the only behavior is
// `--version` (FR-005, FR-006, SC-002, spec Q5). Real `-emit=*` flags
// arrive incrementally from M1 onward and replace the M0 fallthrough.

#include "nsl/Driver/Version.h"

#include <cstdio>
#include <cstring>

namespace {

constexpr const char *kUsage =
    "nslc: usage: nslc --version  (M0 smoke; full CLI lands M1+)\n";

bool isVersionFlag(const char *arg) {
  return std::strcmp(arg, "--version") == 0 ||
         std::strcmp(arg, "-v") == 0 ||
         std::strcmp(arg, "-V") == 0;
}

}  // namespace

int main(int argc, char **argv) {
  if (argc == 2 && isVersionFlag(argv[1])) {
    std::printf("nslc %s\n", NSLC_VERSION_STRING);
    return 0;
  }
  std::fputs(kUsage, stderr);
  return 2;
}
