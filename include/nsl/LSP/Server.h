// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// include/nsl/LSP/Server.h — single public header for `libNSLLSP.a`
// (T3 — tooling-track milestone, `specs/010-t3-lsp-skeleton/`).
//
// Per Constitution Principle II ("each library exposes a single
// public header"), this is the ONLY public header for the NSLLSP
// tooling library. All implementation classes (NslLSPServer,
// NslServer, TUScheduler, NslTU, JSONTransport, DiagnosticMapper,
// FoldingRangeBuilder, PositionEncoding, Logger,
// CancellationToken, IncludeSearchPath) live in `lib/LSP/`
// private headers.
//
// **Specification anchors**:
//   - `specs/010-t3-lsp-skeleton/plan.md` §Summary item 3
//   - `specs/010-t3-lsp-skeleton/contracts/lsp-protocol.contract.md`

#ifndef NSL_LSP_SERVER_H
#define NSL_LSP_SERVER_H

namespace nsl {
namespace lsp {

/// Run an `nsl-lsp` LSP server against stdin/stdout, framed per
/// the LSP base protocol's `Content-Length:` JSON-RPC envelope
/// (LSP 3.16 floor — see Clarifications session 2026-05-05 Q3).
///
/// Reads the `NSL_INCLUDE` and `NSL_LSP_LOG_LEVEL` environment
/// variables ONCE at startup (per FR-020a / FR-020e). Returns a
/// process exit code per `lsp-protocol.contract.md` §9:
///
///   - 0   on clean `shutdown` then `exit`
///   - 1   on `exit` without prior `shutdown`, on stdin EOF, on
///         invalid `NSL_LSP_LOG_LEVEL`, or on uncaught exception
///   - non-zero otherwise per documented exit-code matrix
///
/// `argc` / `argv` are reserved for future flag handling
/// (`--version`, `--help`); at T3 the only consumed flag is
/// `--version`, mirroring `nslc`'s short-circuit.
int runStdioServer(int argc, char **argv);

} // namespace lsp
} // namespace nsl

#endif // NSL_LSP_SERVER_H
