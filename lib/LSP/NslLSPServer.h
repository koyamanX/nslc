// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// lib/LSP/NslLSPServer.h — LSP-protocol layer (clangd-style
// three-layer architecture per `docs/design/nsl_tooling_design.md`
// §3.1). Parses incoming JSON-RPC envelopes via `JSONTransport`,
// dispatches to handlers (lifecycle / sync / feature methods), and
// serializes responses + notifications back to the wire.
//
// **Specification anchors**:
//   - `specs/010-t3-lsp-skeleton/data-model.md` §2.2
//   - `specs/010-t3-lsp-skeleton/contracts/lsp-protocol.contract.md`

#ifndef NSL_LSP_NSL_LSP_SERVER_H
#define NSL_LSP_NSL_LSP_SERVER_H

#include "CancellationToken.h"
#include "RequestId.h"

#include "llvm/ADT/StringMap.h"
#include "llvm/Support/JSON.h"

#include <atomic>
#include <functional>
#include <map>
#include <mutex>
#include <thread>
#include <vector>

namespace nsl {
namespace lsp {

class JSONTransport;
class NslServer;

class NslLSPServer {
public:
  NslLSPServer(JSONTransport &transport, NslServer &backend);

  /// Run the main read-loop until clean shutdown+exit, abnormal
  /// stdin EOF, or fatal error. Returns the process exit code per
  /// contracts/lsp-protocol.contract.md §9.
  int run();

private:
  void dispatch(llvm::json::Value envelope);

  // Lifecycle handlers.
  void onInitialize(const RequestId &id, const llvm::json::Value &params);
  void onInitialized(const llvm::json::Value &params);
  void onShutdown(const RequestId &id);
  void onExit(const llvm::json::Value &params);

  // Document-sync handlers (Phase 2 stubs; Phase 3+4 wire real
  // behavior).
  void onDidOpen(const llvm::json::Value &params);
  void onDidChange(const llvm::json::Value &params);
  void onDidClose(const llvm::json::Value &params);

  // Feature handlers.
  void onFoldingRange(const RequestId &id, const llvm::json::Value &params);

  // Cancellation.
  void onCancelRequest(const llvm::json::Value &params);

  // Response helpers.
  void sendResponse(const RequestId &id, llvm::json::Value result);
  void sendError(const RequestId &id, int code, llvm::StringRef message);
  void sendNotification(llvm::StringRef method, llvm::json::Value params);
  void publishDiagnostics(llvm::StringRef uri, int version,
                          llvm::json::Array diagnostics);

  JSONTransport &transport_;
  NslServer &backend_;

  bool initialized_ = false;
  bool shutdown_received_ = false;
  std::atomic<int> exit_code_{0};
  std::atomic<bool> exited_{false};

  // In-flight cancellable requests. Keyed by RequestId (variant
  // int|string per JSON-RPC). Mutex-protected because
  // `$/cancelRequest` (read+flip) can race with handler completion
  // (remove).
  std::mutex inflight_mtx_;
  std::map<RequestId, CancellationToken> inflight_;

  // Worker threads spawned for cancellable handlers (foldingRange).
  // Joined when the run-loop exits so process teardown is clean
  // even if a stray request was in flight at shutdown time.
  std::mutex workers_mtx_;
  std::vector<std::thread> workers_;
};

} // namespace lsp
} // namespace nsl

#endif // NSL_LSP_NSL_LSP_SERVER_H
