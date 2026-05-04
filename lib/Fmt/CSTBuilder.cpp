// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// lib/Fmt/CSTBuilder.cpp — implementation of the CSTSink that
// records parser events for the formatter (T2 Phase 2b — T019).

#include "CSTBuilder.h"

#include "nsl/Basic/SourceLocation.h"
#include "nsl/Lex/Token.h"

#include "llvm/ADT/StringRef.h"

#include <cstdint>
#include <string>

namespace nsl::fmt {

void CSTBuilder::beginNode(llvm::StringRef kindName,
                           const ::nsl::SourceLocation &start) {
  Frame f;
  f.kindName = kindName.str();
  f.start    = start;
  // `end` is filled in by the matching `endNode()`.
  openStack_.push_back(std::move(f));
}

void CSTBuilder::recordToken(const ::nsl::Token &tok) {
  tokens_.push_back(tok);
}

void CSTBuilder::endNode(const ::nsl::SourceLocation &end) {
  // Tolerate spurious endNode calls (defensive — should not happen
  // with a well-formed parser, but a parse error path that bails
  // mid-production could leave the stack imbalanced).
  if (openStack_.empty()) {
    return;
  }
  Frame f = openStack_.pop_back_val();
  f.end   = end;
  completedNodes_.push_back(std::move(f));
}

std::string CSTBuilder::serialize() const {
  std::string out;
  out.reserve(src_.size());

  std::uint32_t cursor = 0;
  for (const ::nsl::Token &t : tokens_) {
    std::uint32_t tokBegin = t.range().begin().offset();
    std::uint32_t tokEnd   = t.range().end().offset();

    // Emit any inter-token trivia (whitespace, comments) the lexer
    // skipped between the previous token's end and this token's
    // begin. For Phase 2b the lexer does not emit trivia tokens
    // explicitly; we recover them by slicing the source.
    if (tokBegin > cursor && tokBegin <= src_.size()) {
      out.append(src_.data() + cursor, tokBegin - cursor);
    }
    // Emit the token's bytes verbatim from the source view.
    if (tokEnd > tokBegin && tokEnd <= src_.size()) {
      out.append(src_.data() + tokBegin, tokEnd - tokBegin);
    }
    cursor = tokEnd;
  }
  // Emit any trailing source bytes after the last token (e.g. final
  // newline, EOF whitespace).
  if (cursor < src_.size()) {
    out.append(src_.data() + cursor, src_.size() - cursor);
  }

  return out;
}

} // namespace nsl::fmt
