// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// lib/LSP/NslLSPServer.cpp — LSP-protocol layer impl.
//
// Lifecycle (initialize / initialized / shutdown / exit) is fully
// wired here. Document-sync handlers (didOpen / didChange /
// didClose) are Phase 2 stubs that delegate to the backend
// `NslServer` but do not publish diagnostics yet — that wiring
// arrives at T071 (Phase 3 / US1) once `DiagnosticMapper` lands.

#include "NslLSPServer.h"
#include "DiagnosticMapper.h"
#include "FoldingRangeBuilder.h"
#include "JSONTransport.h"
#include "Logger.h"
#include "NslServer.h"

#include "llvm/Support/FormatVariadic.h"
#include "llvm/Support/JSON.h"

#include "nsl/Driver/Version.h"

namespace nsl {
namespace lsp {

namespace {

// JSON-RPC standard error codes (JSON-RPC 2.0 §5.1) and LSP-
// specific codes (LSP 3.16 §base-protocol).
constexpr int kInvalidRequest = -32600;
constexpr int kMethodNotFound = -32601;
constexpr int kServerNotInitialized = -32002;
constexpr int kRequestCancelled = -32800;

llvm::json::Object buildCapabilities() {
  // Per contracts/lsp-protocol.contract.md §1.2 — exact, byte-for-
  // byte stable. Insertion order matters for the canonical wire
  // form (llvm::json::Object preserves insertion order).
  // Order chosen alphabetically.
  return llvm::json::Object{
      {"foldingRangeProvider", true},
      {"textDocumentSync", llvm::json::Object{
                                {"change", 1},
                                {"openClose", true},
                                {"save", false},
                                {"willSave", false},
                                {"willSaveWaitUntil", false},
                            }},
  };
}

llvm::json::Object buildInitializeResult() {
  return llvm::json::Object{
      {"capabilities", buildCapabilities()},
      {"serverInfo", llvm::json::Object{
                          {"name", "nsl-lsp"},
                          {"version", NSLC_VERSION_STRING},
                      }},
  };
}

} // namespace

NslLSPServer::NslLSPServer(JSONTransport &transport, NslServer &backend)
    : transport_(transport), backend_(backend) {
  // Diagnostics-publication path: TUScheduler invokes this callback
  // once a parse + sema cycle completes. We pull the diagnostics
  // and SourceManager from the State, run them through the
  // DiagnosticMapper (T068), and write the resulting LSP
  // publishDiagnostics envelope. Per the harness contract §3.1,
  // diagnostics are sorted by (line, character, severity) inside
  // toLspDiagnosticArray.
  backend_.scheduler().setOnDiagnostics(
      [this](llvm::StringRef uri, int version,
              const NslTU::State &state) {
        if (!state.source_manager) {
          publishDiagnostics(uri, version, llvm::json::Array{});
          return;
        }
        publishDiagnostics(
            uri, version,
            toLspDiagnosticArray(state.diagnostics, *state.source_manager));
      });
}

int NslLSPServer::run() {
  int code = 0;
  while (!exited_.load(std::memory_order_acquire)) {
    auto envelope = transport_.readMessage();
    if (!envelope) {
      // EOF or framing error. If we got a clean shutdown+exit
      // earlier, exit_code_ is 0; otherwise treat as abnormal
      // termination (exit code 1).
      if (!exited_.load(std::memory_order_acquire)) {
        if (!shutdown_received_) {
          NSL_LSP_LOG_WARN("nsl-lsp: stdin EOF without prior "
                            "shutdown; exiting with code 1");
          code = 1;
          break;
        }
        // Shutdown received but no exit notification — treat as 1
        // per contract §9.
        code = 1;
        break;
      }
      break;
    }
    dispatch(std::move(*envelope));
  }
  // Drain any in-flight worker threads so their wire writes complete
  // before the transport stream is torn down by the process exit.
  // Cancel any still-running tokens first so polling-aware handlers
  // unwind quickly instead of finishing their full walk.
  {
    std::lock_guard<std::mutex> guard(inflight_mtx_);
    for (auto &kv : inflight_) {
      kv.second.cancel();
    }
  }
  std::vector<std::thread> to_join;
  {
    std::lock_guard<std::mutex> guard(workers_mtx_);
    to_join = std::move(workers_);
  }
  for (auto &t : to_join) {
    if (t.joinable()) t.join();
  }
  if (code != 0) return code;
  return exit_code_.load(std::memory_order_acquire);
}

void NslLSPServer::dispatch(llvm::json::Value envelope) {
  auto *obj = envelope.getAsObject();
  if (!obj) {
    NSL_LSP_LOG_ERROR("nsl-lsp: dispatch received non-object envelope");
    return;
  }

  auto method_str = obj->getString("method");
  if (!method_str) {
    // Could be a response from the client (LSP allows server-
    // initiated requests; T3 doesn't issue any, so any envelope
    // without `method` is unexpected). Log and ignore.
    NSL_LSP_LOG_DEBUG("nsl-lsp: dispatch saw envelope without method");
    return;
  }
  llvm::StringRef method = *method_str;

  auto *id_val = obj->get("id");
  std::optional<RequestId> id;
  if (id_val) id = RequestId::fromJson(*id_val);

  llvm::json::Value params_default(nullptr);
  llvm::json::Value &params =
      obj->get("params") ? *obj->get("params") : params_default;

  // Pre-`initialized` gate: only `initialize`, `initialized`,
  // `shutdown`, `exit`, and notifications starting with `$/` are
  // permitted. Per contract §1.3, every other request gets
  // ServerNotInitialized; notifications are silently ignored.
  // (`shutdown` is allowed pre-`initialized` because the LSP
  // lifecycle permits an immediate teardown after initialize —
  // clangd matches this behavior.)
  if (!initialized_ && method != "initialize" && method != "initialized" &&
      method != "shutdown" && method != "exit" &&
      !method.starts_with("$/")) {
    if (id) {
      sendError(*id, kServerNotInitialized,
                 "server not initialized: send 'initialize' first");
    } else {
      NSL_LSP_LOG_DEBUG(llvm::formatv(
          "nsl-lsp: dropping pre-initialized notification {0}",
          method).str());
    }
    return;
  }

  // After shutdown, only `exit` and `$/cancelRequest` are honored.
  if (shutdown_received_ && method != "exit" && !method.starts_with("$/")) {
    if (id) {
      sendError(*id, kInvalidRequest,
                 "request received after shutdown");
    }
    return;
  }

  // Method dispatch.
  if (method == "initialize") {
    if (!id) {
      NSL_LSP_LOG_ERROR("nsl-lsp: initialize received without id");
      return;
    }
    onInitialize(*id, params);
  } else if (method == "initialized") {
    onInitialized(params);
  } else if (method == "shutdown") {
    if (!id) {
      NSL_LSP_LOG_ERROR("nsl-lsp: shutdown received without id");
      return;
    }
    onShutdown(*id);
  } else if (method == "exit") {
    onExit(params);
  } else if (method == "textDocument/didOpen") {
    onDidOpen(params);
  } else if (method == "textDocument/didChange") {
    onDidChange(params);
  } else if (method == "textDocument/didClose") {
    onDidClose(params);
  } else if (method == "textDocument/foldingRange") {
    if (!id) {
      NSL_LSP_LOG_ERROR("nsl-lsp: foldingRange received without id");
      return;
    }
    onFoldingRange(*id, params);
  } else if (method == "$/cancelRequest") {
    onCancelRequest(params);
  } else {
    if (id) sendError(*id, kMethodNotFound,
                        llvm::formatv("method not found: {0}", method).str());
    // Notifications (no id) are silently dropped per LSP.
  }
}

void NslLSPServer::onInitialize(const RequestId &id,
                                  const llvm::json::Value &params) {
  // Log INFO with optional clientInfo per contract §7.4.
  if (auto *obj = params.getAsObject()) {
    if (auto *ci = obj->getObject("clientInfo")) {
      auto name = ci->getString("name").value_or("<unknown>");
      auto version = ci->getString("version").value_or("");
      NSL_LSP_LOG_INFO(llvm::formatv("initialize received from {0} {1}",
                                       name, version).str());
    } else {
      NSL_LSP_LOG_INFO("initialize received");
    }
  } else {
    NSL_LSP_LOG_INFO("initialize received");
  }

  sendResponse(id, buildInitializeResult());
}

void NslLSPServer::onInitialized(const llvm::json::Value & /*params*/) {
  initialized_ = true;
  NSL_LSP_LOG_INFO("initialized notification received; ready");
}

void NslLSPServer::onShutdown(const RequestId &id) {
  NSL_LSP_LOG_INFO("shutdown received; draining pending parse work");
  shutdown_received_ = true;

  backend_.waitForIdle();
  // Per contract §5.1, emit one final empty publishDiagnostics for
  // every still-open document. At Phase 2 the document map is
  // owned by the backend; we don't enumerate it from the protocol
  // layer at this milestone — Phase 4 (T080) wires the per-URI
  // close-time empty publish from didClose.
  sendResponse(id, llvm::json::Value(nullptr));
}

void NslLSPServer::onExit(const llvm::json::Value & /*params*/) {
  NSL_LSP_LOG_INFO("exit notification received");
  exit_code_.store(shutdown_received_ ? 0 : 1, std::memory_order_release);
  exited_.store(true, std::memory_order_release);
}

void NslLSPServer::onDidOpen(const llvm::json::Value &params) {
  auto *obj = params.getAsObject();
  if (!obj) {
    NSL_LSP_LOG_ERROR("didOpen: malformed params (not an object)");
    return;
  }
  auto *td = obj->getObject("textDocument");
  if (!td) {
    NSL_LSP_LOG_ERROR("didOpen: malformed params (missing textDocument)");
    return;
  }
  auto uri = td->getString("uri").value_or("");
  auto text = td->getString("text").value_or("");
  int64_t version = td->getInteger("version").value_or(0);
  if (uri.empty()) {
    NSL_LSP_LOG_ERROR("didOpen: empty uri");
    return;
  }
  backend_.openOrUpdate(uri, static_cast<int>(version), text.str());
}

void NslLSPServer::onDidChange(const llvm::json::Value &params) {
  auto *obj = params.getAsObject();
  if (!obj) {
    NSL_LSP_LOG_ERROR("didChange: malformed params (not an object)");
    return;
  }
  auto *td = obj->getObject("textDocument");
  auto *changes = obj->getArray("contentChanges");
  if (!td || !changes) {
    NSL_LSP_LOG_ERROR("didChange: malformed params");
    return;
  }
  auto uri = td->getString("uri").value_or("");
  int64_t version = td->getInteger("version").value_or(0);
  if (uri.empty()) {
    NSL_LSP_LOG_ERROR("didChange: empty uri");
    return;
  }
  if (changes->size() != 1) {
    NSL_LSP_LOG_ERROR(llvm::formatv(
        "didChange: expected exactly 1 contentChange (Full sync), got {0}",
        changes->size()).str());
    return;
  }
  auto *change = (*changes)[0].getAsObject();
  if (!change) {
    NSL_LSP_LOG_ERROR("didChange: contentChange is not an object");
    return;
  }
  if (change->get("range") != nullptr) {
    NSL_LSP_LOG_ERROR("didChange: contentChange carries 'range' but the "
                       "server advertised TextDocumentSyncKind.Full; "
                       "rejecting");
    return;
  }
  // Stale-version check per contract §2.2 / FR-008: ignore
  // didChange notifications whose version is <= the TU's
  // last-known version. The check goes through the TU mutex via
  // withState.
  int last_known = -1;
  backend_.scheduler().withState(uri, [&](const NslTU::State &st) {
    last_known = st.version;
  });
  if (last_known >= 0 && version <= last_known) {
    NSL_LSP_LOG_WARN(llvm::formatv(
        "didChange: stale version {0} (last-known {1}); ignoring",
        static_cast<int>(version), last_known).str());
    return;
  }
  auto text = change->getString("text").value_or("");
  backend_.openOrUpdate(uri, static_cast<int>(version), text.str());
}

void NslLSPServer::onDidClose(const llvm::json::Value &params) {
  auto *obj = params.getAsObject();
  if (!obj) return;
  auto *td = obj->getObject("textDocument");
  if (!td) return;
  auto uri = td->getString("uri").value_or("");
  if (uri.empty()) return;

  // Per contract §2.3 / FR-007: emit one final empty diagnostics
  // notification before tearing down the TU, using the URI's
  // last-known version.
  int last_version = 0;
  backend_.scheduler().withState(
      uri, [&](const NslTU::State &state) { last_version = state.version; });
  publishDiagnostics(uri, last_version, llvm::json::Array{});
  backend_.close(uri);
}

void NslLSPServer::onFoldingRange(const RequestId &id,
                                    const llvm::json::Value &params) {
  // Extract URI.
  auto *obj = params.getAsObject();
  if (!obj) {
    sendResponse(id, llvm::json::Array{});
    return;
  }
  auto *td = obj->getObject("textDocument");
  auto uri = td ? td->getString("uri").value_or("") : llvm::StringRef("");

  // Register a cancellation token in the in-flight table.
  CancellationToken token = CancellationToken::make();
  {
    std::lock_guard<std::mutex> guard(inflight_mtx_);
    inflight_[id] = token;
  }

  // Run the folding-range walk on a worker thread so the dispatch
  // thread can continue reading messages — in particular, an
  // `$/cancelRequest` notification that arrives mid-walk MUST be
  // observable by the worker's polling discipline (FR-020h–j /
  // folding-range contract §5 / SC-010). Without offloading, the
  // dispatch thread would block until the walk completes and the
  // cancellation flag would never flip during the walk.
  std::string contents;
  backend_.scheduler().withState(
      uri, [&](const NslTU::State &st) { contents = st.contents; });

  std::thread worker([this, id, contents = std::move(contents),
                      token = std::move(token)]() mutable {
    llvm::json::Array folds = buildFoldingRanges(contents, token);

    bool was_cancelled;
    {
      std::lock_guard<std::mutex> guard(inflight_mtx_);
      inflight_.erase(id);
      was_cancelled = token.isCancelled();
    }

    if (was_cancelled) {
      sendError(id, kRequestCancelled, "request cancelled");
    } else {
      sendResponse(id, std::move(folds));
    }
  });
  std::lock_guard<std::mutex> wguard(workers_mtx_);
  workers_.push_back(std::move(worker));
}

void NslLSPServer::onCancelRequest(const llvm::json::Value &params) {
  auto *obj = params.getAsObject();
  if (!obj) return;
  auto *id_val = obj->get("id");
  if (!id_val) return;
  auto id = RequestId::fromJson(*id_val);
  if (!id) return;

  std::lock_guard<std::mutex> guard(inflight_mtx_);
  auto it = inflight_.find(*id);
  if (it == inflight_.end()) {
    NSL_LSP_LOG_DEBUG("cancelRequest: id not in flight (already "
                       "completed, never seen, or notification)");
    return;
  }
  it->second.cancel();
}

void NslLSPServer::sendResponse(const RequestId &id,
                                  llvm::json::Value result) {
  // JSON-RPC envelope key order chosen for the canonical wire
  // form (id < jsonrpc < result alphabetically).
  transport_.writeMessage(llvm::json::Object{
      {"id", id.toJson()},
      {"jsonrpc", "2.0"},
      {"result", std::move(result)},
  });
}

void NslLSPServer::sendError(const RequestId &id, int code,
                                llvm::StringRef message) {
  transport_.writeMessage(llvm::json::Object{
      {"error", llvm::json::Object{
                     {"code", code},
                     {"message", message.str()},
                 }},
      {"id", id.toJson()},
      {"jsonrpc", "2.0"},
  });
}

void NslLSPServer::sendNotification(llvm::StringRef method,
                                      llvm::json::Value params) {
  transport_.writeMessage(llvm::json::Object{
      {"jsonrpc", "2.0"},
      {"method", method.str()},
      {"params", std::move(params)},
  });
}

void NslLSPServer::publishDiagnostics(llvm::StringRef uri, int version,
                                        llvm::json::Array diagnostics) {
  sendNotification("textDocument/publishDiagnostics",
                    llvm::json::Object{
                        {"diagnostics", std::move(diagnostics)},
                        {"uri", uri.str()},
                        {"version", version},
                    });
}

} // namespace lsp
} // namespace nsl
