// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// lib/Fmt/Doc.cpp — factory implementations for the Wadler-Leijen
// pretty-printer IR (T2 Phase 2c — T024).

#include "Doc.h"

#include "llvm/ADT/StringRef.h"

#include <initializer_list>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace nsl::fmt {

DocPtr Doc::text(llvm::StringRef s) {
  return std::make_shared<const Doc>(
      doc_detail::DocText{std::string(s)});
}

DocPtr Doc::line() {
  return std::make_shared<const Doc>(doc_detail::DocLine{false});
}

DocPtr Doc::hardline() {
  return std::make_shared<const Doc>(doc_detail::DocLine{true});
}

DocPtr Doc::nest(int indent, DocPtr inner) {
  return std::make_shared<const Doc>(
      doc_detail::DocNest{indent, std::move(inner)});
}

DocPtr Doc::group(DocPtr inner) {
  return std::make_shared<const Doc>(
      doc_detail::DocGroup{std::move(inner)});
}

DocPtr Doc::concat(std::initializer_list<DocPtr> items) {
  return std::make_shared<const Doc>(
      doc_detail::DocConcat{std::vector<DocPtr>(items.begin(), items.end())});
}

DocPtr Doc::concat(std::vector<DocPtr> items) {
  return std::make_shared<const Doc>(
      doc_detail::DocConcat{std::move(items)});
}

DocPtr Doc::align(DocPtr inner) {
  return std::make_shared<const Doc>(
      doc_detail::DocAlign{std::move(inner)});
}

DocPtr Doc::comment(llvm::StringRef text, bool leading, bool trailing) {
  return std::make_shared<const Doc>(
      doc_detail::DocComment{std::string(text), leading, trailing});
}

} // namespace nsl::fmt
