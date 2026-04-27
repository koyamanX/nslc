// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// lib/Basic/SourceLocation.cpp — implementation of the foundational
// source-attribution types declared in
// `include/nsl/Basic/SourceLocation.h`. Per data-model entities 1–3,
// the types are deliberately small (32 bits per `SourceLocation`,
// 64 bits per `SourceRange`) so Tokens stay cache-friendly.

#include "nsl/Basic/SourceLocation.h"

#include "AssertImpl.h"

namespace nsl {

SourceLocation SourceLocation::make(FileID fid, uint32_t off) {
  // Hard-fail on offset overflow: at M1 no NSL source plausibly
  // exceeds 16 MiB; if a future user hits the limit, the right
  // answer is to widen the field, not silently truncate (data-model
  // entity 1 invariant).
  NSL_ABORT(off < kMaxOffset, "SourceLocation offset >= 16 MiB");

  SourceLocation result;
  result.bits_ = (static_cast<uint32_t>(fid.raw()) << 24) | (off & 0x00FFFFFFU);
  return result;
}

SourceRange::SourceRange(SourceLocation b, SourceLocation e)
    : begin_(b), end_(e) {
  // Same-file invariant per data-model entity 2.
  NSL_ABORT(b.file() == e.file(),
            "SourceRange endpoints must be in the same file");
  // Half-open: begin <= end.
  NSL_ABORT(!(e < b), "SourceRange begin must be <= end");
}

} // namespace nsl
