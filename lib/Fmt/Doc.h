//===- Doc.h - Wadler-Leijen pretty-printer IR for nsl-fmt ------*- C++ -*-=//
//
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Internal header — NOT exported through `Fmt.h` (Principle II).
//
// Implements the Wadler-Leijen layout-IR primitives sketched in
// `docs/design/nsl_tooling_design.md` §5.2 and frozen by
// `specs/010-t2-formatter-v0/data-model.md` §4. The seven layout
// constructors are:
//
//   Text    — literal string
//   Line    — soft break (becomes single space when the enclosing
//             Group fits on one line; becomes "\n" + indent
//             otherwise). `hardline()` is the always-break variant
//             (still Kind::Line at the variant level — it just
//             carries `isHard = true`).
//   Nest    — increase indent by N for the contained doc
//   Group   — try to fit the contained doc on one line; if it does
//             not fit within `max_line_length - column`, fall back
//             to break-at-every-Line mode
//   Concat  — sequence of docs (rendered in order)
//   Align   — align the contained doc to the current column
//   Comment — preserves a comment + its surrounding trivia (rendered
//             as if Text for layout purposes)
//
// Doc instances are immutable after construction. Storage is
// `std::shared_ptr<Doc>` (matches the project's tree-shaped IR
// ownership convention used by `lib/AST/`).
//
//===----------------------------------------------------------------------===//

#ifndef NSL_FMT_LIB_DOC_H
#define NSL_FMT_LIB_DOC_H

#include "llvm/ADT/StringRef.h"

#include <initializer_list>
#include <memory>
#include <string>
#include <variant>
#include <vector>

namespace nsl::fmt {

class Doc;
using DocPtr = std::shared_ptr<const Doc>;

// ----- Per-kind payload structs ----------------------------------

namespace doc_detail {

struct DocText {
  std::string text;        // owns the rendered bytes
};

struct DocLine {
  bool isHard;             // hardline() => true; line() => false
};

struct DocNest {
  int    indent;
  DocPtr inner;
};

struct DocGroup {
  DocPtr inner;
};

struct DocConcat {
  std::vector<DocPtr> items;
};

struct DocAlign {
  DocPtr inner;
};

struct DocComment {
  std::string text;
  bool        leading;     // attaches above the next non-trivia
  bool        trailing;    // attaches to the previous non-trivia line
};

} // namespace doc_detail

class Doc {
public:
  using Payload =
      std::variant<doc_detail::DocText, doc_detail::DocLine,
                   doc_detail::DocNest, doc_detail::DocGroup,
                   doc_detail::DocConcat, doc_detail::DocAlign,
                   doc_detail::DocComment>;

  /// Stable kind tag. Matches the variant index 1:1.
  enum class Kind : std::size_t {
    Text    = 0,
    Line    = 1,
    Nest    = 2,
    Group   = 3,
    Concat  = 4,
    Align   = 5,
    Comment = 6,
  };

  // ---- Factories (data-model §4 names) --------------------------

  static DocPtr text(llvm::StringRef s);
  static DocPtr line();          // soft break
  static DocPtr hardline();      // always break
  static DocPtr nest(int indent, DocPtr inner);
  static DocPtr group(DocPtr inner);
  static DocPtr concat(std::initializer_list<DocPtr> items);
  static DocPtr concat(std::vector<DocPtr> items);
  static DocPtr align(DocPtr inner);
  static DocPtr comment(llvm::StringRef text, bool leading, bool trailing);

  // ---- Accessors ------------------------------------------------

  [[nodiscard]] Kind kind() const noexcept {
    return static_cast<Kind>(payload_.index());
  }

  template <typename T>
  [[nodiscard]] const T *as() const noexcept {
    return std::get_if<T>(&payload_);
  }

  /// Internal access for the renderer.
  [[nodiscard]] const Payload &payload() const noexcept { return payload_; }

  // ---- Construction ----------------------------------------------
  //
  // Public constructor exists because `std::make_shared<Doc>` calls
  // it, but the factories above are the canonical entry points.
  explicit Doc(Payload p) noexcept(false) : payload_(std::move(p)) {}

private:
  Payload payload_;
};

} // namespace nsl::fmt

#endif // NSL_FMT_LIB_DOC_H
