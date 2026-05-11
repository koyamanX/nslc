// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// lib/LSP/NslServer.h — language-logic layer (clangd-style three-
// layer architecture per `docs/design/nsl_tooling_design.md` §3.1).
// Stateless API exposing the language operations the protocol
// layer dispatches to: diagnostics callback wiring, AST access for
// folding, future T4/T5/T9/T10 features.
//
// **Specification anchors**:
//   - `specs/010-t3-lsp-skeleton/data-model.md` §2.3

#ifndef NSL_LSP_NSL_SERVER_H
#define NSL_LSP_NSL_SERVER_H

#include "IncludeSearchPath.h"
#include "TUScheduler.h"

#include "llvm/ADT/StringRef.h"

namespace nsl {
namespace lsp {

class NslServer {
public:
  explicit NslServer(IncludeSearchPath includes, unsigned workers);

  TUScheduler &scheduler() { return scheduler_; }
  const IncludeSearchPath &includes() const { return includes_; }

  /// `didOpen` / `didChange` land here.
  void openOrUpdate(llvm::StringRef uri, int version, std::string contents);

  /// `didClose` lands here.
  void close(llvm::StringRef uri);

  /// `shutdown` drains pending parse work.
  void waitForIdle() { scheduler_.waitForIdle(); }

private:
  IncludeSearchPath includes_;
  TUScheduler scheduler_;
};

} // namespace lsp
} // namespace nsl

#endif // NSL_LSP_NSL_SERVER_H
