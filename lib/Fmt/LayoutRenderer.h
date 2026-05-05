//===- LayoutRenderer.h - Doc IR → text renderer ----------------*- C++ -*-=//
//
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Internal header — NOT exported through `Fmt.h` (Principle II).
//
// Renders a `Doc` tree to a flat byte string by deciding, for each
// `Doc::group(inner)`, whether `inner` fits on one line (flat
// rendering) or must break (one break per `Line` inside). The
// algorithm is the standard Wadler-Leijen "ribbon-fit" walk:
//
//   1. For each `Group`, attempt a flat layout: walk inner with
//      `mode = Flat` and an "available width" budget initialised
//      to `max_line_length - current_column`.
//   2. If the flat walk reaches its end without exceeding the
//      budget AND without encountering a hardline, emit `inner`
//      flat.
//   3. Otherwise, emit `inner` in `mode = Break`: every soft `Line`
//      becomes "\n" + current-indent.
//
// Output is deterministic: the algorithm's only environment
// dependence is the `Configuration` value (max_line_length, indent
// width, etc.) supplied by the caller. Per Principle V byte-
// stability, the renderer reads no environment, no clock, no
// pointer bits.
//
//===----------------------------------------------------------------------===//

#ifndef NSL_FMT_LIB_LAYOUT_RENDERER_H
#define NSL_FMT_LIB_LAYOUT_RENDERER_H

#include "Doc.h"

#include <string>

namespace nsl::fmt {

class LayoutRenderer {
public:
  /// Render `doc` to a byte string.
  ///
  /// `maxLineLength` is the ribbon width — the renderer breaks any
  /// `Group` whose flat layout would exceed `maxLineLength - column`.
  /// `indentSpaces` is the per-`Nest` step in space characters; pass
  /// negative values (e.g. `-1`) to emit literal `\t` characters
  /// (the rendering equivalent of `Configuration::Indent::Tab`).
  ///
  /// Returns the rendered text. Determinism: pure function of
  /// `(doc, maxLineLength, indentSpaces)` (Principle V).
  std::string render(const DocPtr &doc, int maxLineLength,
                     int indentSpaces) const;
};

} // namespace nsl::fmt

#endif // NSL_FMT_LIB_LAYOUT_RENDERER_H
