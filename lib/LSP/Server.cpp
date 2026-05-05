// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// lib/LSP/Server.cpp — top-level entry point for the `nsl-lsp`
// LSP server.
//
// At T3 Phase 1 (skeleton) this is a stub returning 0 immediately
// without I/O or lifecycle handling. Phase 2 (T024–T029) wires the
// real flow: Logger init from NSL_LSP_LOG_LEVEL → IncludeSearchPath
// from NSL_INCLUDE → JSONTransport over stdin/stdout → NslServer →
// NslLSPServer.run().
//
// **Specification anchors**:
//   - `specs/010-t3-lsp-skeleton/plan.md` §Summary
//   - `specs/010-t3-lsp-skeleton/contracts/lsp-protocol.contract.md`

#include "nsl/LSP/Server.h"

namespace nsl {
namespace lsp {

int runStdioServer(int /*argc*/, char ** /*argv*/) {
  // Phase 1 stub. Phase 2 onwards lands the real flow per
  // contracts/lsp-protocol.contract.md.
  return 0;
}

} // namespace lsp
} // namespace nsl
