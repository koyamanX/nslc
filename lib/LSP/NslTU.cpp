// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// lib/LSP/NslTU.cpp — per-document state impl. Phase 2 stub; the
// real `Compilation` invocation lands at T069 (Phase 3 / US1).

#include "NslTU.h"
#include "IncludeSearchPath.h"

namespace nsl {
namespace lsp {

NslTU::NslTU() = default;
NslTU::~NslTU() = default;

int NslTU::reparse(int version, std::string contents,
                    const IncludeSearchPath & /*includes*/) {
  std::lock_guard<std::mutex> guard(mtx_);
  state_.version = version;
  state_.contents = std::move(contents);
  // Phase 2 stub: no parse, no sema. T069 (Phase 3) wires the real
  // `nsl::driver::Compilation` invocation through the includes.
  state_.ast.reset();
  state_.symbols.reset();
  state_.diagnostics.clear();
  return version;
}

int NslTU::latestVersion() const {
  std::lock_guard<std::mutex> guard(mtx_);
  return state_.version;
}

} // namespace lsp
} // namespace nsl
