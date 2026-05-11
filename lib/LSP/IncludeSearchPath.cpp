// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// lib/LSP/IncludeSearchPath.cpp — `NSL_INCLUDE` env-var parser.

#include "IncludeSearchPath.h"

#include "Logger.h"

#include "llvm/Support/FormatVariadic.h"

#include <cstdlib>
#include <string>
#include <vector>

namespace nsl {
namespace lsp {

namespace {

#ifdef _WIN32
constexpr char kPathSep = ';';
#else
constexpr char kPathSep = ':';
#endif

std::vector<std::string> splitPath(const char *value) {
  std::vector<std::string> out;
  if (!value || !*value)
    return out;
  const char *start = value;
  for (const char *p = value;; ++p) {
    if (*p == kPathSep || *p == '\0') {
      if (p > start)
        out.emplace_back(start, p);
      if (*p == '\0')
        break;
      start = p + 1;
    }
  }
  return out;
}

} // namespace

IncludeSearchPath IncludeSearchPath::fromEnv() {
  const char *raw = std::getenv("NSL_INCLUDE");
  IncludeSearchPath path(splitPath(raw));

  if (path.empty()) {
    NSL_LSP_LOG_INFO("NSL_INCLUDE unset or empty; angle-form "
                     "include search path is empty");
  } else {
    std::string joined;
    for (const auto &p : path.anglePaths()) {
      if (!joined.empty())
        joined.push_back(kPathSep);
      joined += p;
    }
    NSL_LSP_LOG_INFO(llvm::formatv("NSL_INCLUDE resolved: {0}", joined).str());
  }
  return path;
}

} // namespace lsp
} // namespace nsl
