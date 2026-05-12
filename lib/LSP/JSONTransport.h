// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// lib/LSP/JSONTransport.h — `Content-Length:`-framed JSON-RPC
// transport for `nsl-lsp`. Hand-rolled (per research §R1) to keep
// dependencies minimal and to own the cancellation-token plumbing
// locally. Uses `llvm::json::Value` for parsing and serialization.
//
// **Specification anchors**:
//   - `specs/010-t3-lsp-skeleton/research.md` §R1
//   - `specs/010-t3-lsp-skeleton/data-model.md` §2.1
//   - LSP 3.16 base protocol — Content-Length framing
//
// Thread safety: `writeMessage` is mutex-protected (atomic at the
// JSON-RPC envelope granularity). `readMessage` is single-reader
// only — callers MUST NOT invoke it concurrently.

#ifndef NSL_LSP_JSON_TRANSPORT_H
#define NSL_LSP_JSON_TRANSPORT_H

#include "llvm/Support/JSON.h"

#include <iosfwd>
#include <mutex>
#include <optional>

namespace nsl {
namespace lsp {

class JSONTransport {
public:
  JSONTransport(std::istream &in, std::ostream &out);

  /// Read the next JSON-RPC envelope. Returns the parsed JSON value
  /// on success. Returns std::nullopt on EOF or on any framing
  /// error (logged at ERROR per contract §7.4 — bad Content-Length,
  /// malformed JSON, missing separator, etc.). After a framing
  /// error the transport is unrecoverable; callers SHOULD terminate
  /// the read loop.
  std::optional<llvm::json::Value> readMessage();

  /// Serialize `msg` and write it framed with the LSP base
  /// protocol's `Content-Length:` header. Thread-safe.
  void writeMessage(const llvm::json::Value &msg);

private:
  std::istream &in_;
  std::ostream &out_;
  std::mutex write_mtx_;
};

} // namespace lsp
} // namespace nsl

#endif // NSL_LSP_JSON_TRANSPORT_H
