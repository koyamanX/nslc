// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// lib/LSP/RequestId.h — JSON-RPC 2.0 request identifier. Per the
// JSON-RPC spec, an `id` is a number, string, or null; LSP
// constrains it to integer or string (never null).
// We model it as `std::variant<int64_t, std::string>` and provide
// equality + ordering for use as a `std::map` key in the in-flight
// request table.
//
// **Specification anchors**:
//   - `specs/010-t3-lsp-skeleton/data-model.md` §2.8
//   - JSON-RPC 2.0 §4 (Request object)

#ifndef NSL_LSP_REQUEST_ID_H
#define NSL_LSP_REQUEST_ID_H

#include "llvm/Support/JSON.h"

#include <cstdint>
#include <optional>
#include <string>
#include <variant>

namespace nsl {
namespace lsp {

struct RequestId {
  std::variant<int64_t, std::string> value;

  RequestId() = default;
  /*implicit*/ RequestId(int64_t i) : value(i) {}
  /*implicit*/ RequestId(std::string s) : value(std::move(s)) {}

  bool operator==(const RequestId &o) const noexcept {
    return value == o.value;
  }
  bool operator<(const RequestId &o) const noexcept {
    return value < o.value;
  }

  /// Render to its JSON-RPC `id` form (preserves int/string type).
  llvm::json::Value toJson() const {
    if (auto *i = std::get_if<int64_t>(&value)) return *i;
    return std::get<std::string>(value);
  }

  /// Parse from a JSON-RPC `id` value. Returns std::nullopt when
  /// `v` is not an integer or string (i.e., `null` or unexpected
  /// shape).
  static std::optional<RequestId> fromJson(const llvm::json::Value &v) {
    if (auto i = v.getAsInteger()) return RequestId(*i);
    if (auto s = v.getAsString()) return RequestId(std::string(*s));
    return std::nullopt;
  }
};

} // namespace lsp
} // namespace nsl

#endif // NSL_LSP_REQUEST_ID_H
