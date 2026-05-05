// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// lib/LSP/Server.cpp — top-level entry point for the `nsl-lsp`
// LSP server.
//
// Wires the four layers per `docs/design/nsl_tooling_design.md`
// §3.1: JSONTransport → NslLSPServer → NslServer → TUScheduler →
// libNSLFrontend.a (consumed via the existing nsl-driver layer).

#include "nsl/LSP/Server.h"

#include "IncludeSearchPath.h"
#include "JSONTransport.h"
#include "Logger.h"
#include "NslLSPServer.h"
#include "NslServer.h"
#include "TUScheduler.h"

#include "llvm/Support/FormatVariadic.h"

#include <exception>
#include <iostream>

namespace nsl {
namespace lsp {

int runStdioServer(int /*argc*/, char ** /*argv*/) {
  Logger::initFromEnv(); // exits non-zero on invalid value (FR-020e)

  unsigned workers = TUScheduler::workersFromEnv();
  IncludeSearchPath includes = IncludeSearchPath::fromEnv();
  NSL_LSP_LOG_INFO(llvm::formatv("nsl-lsp starting; {0} worker(s)",
                                   workers).str());

  // Configure stdin/stdout for binary-clean operation. The project
  // targets Linux x86_64 only, so std::cin/std::cout default to
  // text mode which already passes bytes through unchanged.
  std::cin.tie(nullptr);

  try {
    NslServer backend(std::move(includes), workers);
    JSONTransport transport(std::cin, std::cout);
    NslLSPServer server(transport, backend);
    return server.run();
  } catch (const std::exception &e) {
    NSL_LSP_LOG_ERROR(llvm::formatv("nsl-lsp: uncaught exception: {0}",
                                      e.what()).str());
    return 1;
  } catch (...) {
    NSL_LSP_LOG_ERROR("nsl-lsp: uncaught non-std exception");
    return 1;
  }
}

} // namespace lsp
} // namespace nsl
