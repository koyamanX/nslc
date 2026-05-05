// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// lib/LSP/NslServer.cpp — language-logic layer impl.

#include "NslServer.h"

namespace nsl {
namespace lsp {

NslServer::NslServer(IncludeSearchPath includes, unsigned workers)
    : includes_(std::move(includes)), scheduler_(workers) {}

void NslServer::openOrUpdate(llvm::StringRef uri, int version,
                                std::string contents) {
  scheduler_.open(uri);
  scheduler_.update(uri, version, std::move(contents), includes_);
}

void NslServer::close(llvm::StringRef uri) { scheduler_.close(uri); }

} // namespace lsp
} // namespace nsl
