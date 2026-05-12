// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// lib/Fmt/LayoutRenderer.cpp — Wadler-Leijen ribbon-fit renderer
// (T2 Phase 2c — T025).
//
// Algorithm:
//   * `render(doc, ...)` walks `doc` once, maintaining a current
//     `column` and a per-call indent stack.
//   * On encountering a `Group`, we first run a `fits(group.inner,
//     budget)` check that simulates the flat layout: it accumulates
//     widths of `Text`, `Comment`, `Concat`, `Nest` payloads, treats
//     soft `Line` as one space, and short-circuits with `false` on
//     the first hardline OR the first overflow.
//   * If the group fits, emit it in `flat` mode (every soft Line
//     becomes one space; hardlines impossible by the precondition).
//   * Otherwise emit it in `break` mode (every soft Line becomes a
//     real break + indent prefix).
//
// Hardlines (`Doc::hardline()`) ALWAYS break — they're invisible to
// the fits() simulator (which short-circuits as "no fit" the moment
// it sees one), so a Group containing a hardline is forced into
// break mode by construction.

#include "LayoutRenderer.h"

#include "Doc.h"

#include <cstddef>
#include <string>
#include <variant>

namespace nsl::fmt {

namespace {

// Render an indent of `indentSpaces` spaces (or one '\t' if
// `indentSpaces < 0`) at the start of a fresh line, repeated
// `indent` indent-levels times. For Phase 2c the renderer treats
// indent as a column count rather than a level count — the Nest
// payload contributes the per-level width directly.
void emitIndent(std::string &out, int columnIndent, int indentSpaces) {
  if (indentSpaces < 0) {
    // Tab mode: emit one `\t` per indent level. `LayoutPlanner::
    // indentStep()` returns 1 for `Indent::Tab`, so `columnIndent`
    // equals the current Doc::nest depth — one literal tab per
    // level keeps nested blocks visually distinguishable.
    for (int i = 0; i < columnIndent; ++i) {
      out.push_back('\t');
    }
    return;
  }
  for (int i = 0; i < columnIndent; ++i) {
    out.push_back(' ');
  }
}

// Width of a single token / leaf in flat-layout simulation.
// Returns `-1` to signal "definitely will not fit" (hardline,
// or a leaf payload longer than the budget — in which case the
// caller short-circuits to break mode).
int flatWidth(const Doc &d, int budget) {
  return std::visit(
      [&](const auto &p) -> int {
        using T = std::decay_t<decltype(p)>;
        if constexpr (std::is_same_v<T, doc_detail::DocText>) {
          return static_cast<int>(p.text.size());
        } else if constexpr (std::is_same_v<T, doc_detail::DocLine>) {
          return p.isHard ? -1 : 1; // soft Line == single space
        } else if constexpr (std::is_same_v<T, doc_detail::DocNest>) {
          if (!p.inner)
            return 0;
          return flatWidth(*p.inner, budget);
        } else if constexpr (std::is_same_v<T, doc_detail::DocGroup>) {
          if (!p.inner)
            return 0;
          return flatWidth(*p.inner, budget);
        } else if constexpr (std::is_same_v<T, doc_detail::DocConcat>) {
          int total = 0;
          for (const DocPtr &item : p.items) {
            if (!item)
              continue;
            int w = flatWidth(*item, budget - total);
            if (w < 0 || total + w > budget) {
              return -1;
            }
            total += w;
          }
          return total;
        } else if constexpr (std::is_same_v<T, doc_detail::DocAlign>) {
          if (!p.inner)
            return 0;
          return flatWidth(*p.inner, budget);
        } else { // DocComment
          return static_cast<int>(p.text.size());
        }
      },
      d.payload());
}

bool fits(const Doc &d, int budget) {
  int w = flatWidth(d, budget);
  return w >= 0 && w <= budget;
}

// Recursive renderer. `mode == true` means flat (soft Line → " ");
// `mode == false` means break (soft Line → "\n" + indent). `column`
// tracks the current emit column; `indent` tracks the current
// indent level (column-equivalent).
void renderInto(std::string &out, const Doc &d, int maxWidth, int indentSpaces,
                int indent, int &column, bool flatMode) {
  std::visit(
      [&](const auto &p) {
        using T = std::decay_t<decltype(p)>;
        if constexpr (std::is_same_v<T, doc_detail::DocText>) {
          out.append(p.text);
          column += static_cast<int>(p.text.size());
        } else if constexpr (std::is_same_v<T, doc_detail::DocLine>) {
          if (flatMode && !p.isHard) {
            out.push_back(' ');
            column += 1;
          } else {
            out.push_back('\n');
            emitIndent(out, indent, indentSpaces);
            column = indent;
          }
        } else if constexpr (std::is_same_v<T, doc_detail::DocNest>) {
          if (p.inner) {
            int newIndent = indent + p.indent;
            renderInto(out, *p.inner, maxWidth, indentSpaces, newIndent, column,
                       flatMode);
          }
        } else if constexpr (std::is_same_v<T, doc_detail::DocGroup>) {
          if (!p.inner)
            return;
          int budget = maxWidth - column;
          bool inner_flat = fits(*p.inner, budget);
          renderInto(out, *p.inner, maxWidth, indentSpaces, indent, column,
                     inner_flat);
        } else if constexpr (std::is_same_v<T, doc_detail::DocConcat>) {
          for (const DocPtr &item : p.items) {
            if (item) {
              renderInto(out, *item, maxWidth, indentSpaces, indent, column,
                         flatMode);
            }
          }
        } else if constexpr (std::is_same_v<T, doc_detail::DocAlign>) {
          if (p.inner) {
            renderInto(out, *p.inner, maxWidth, indentSpaces, column, column,
                       flatMode);
          }
        } else { // DocComment
          out.append(p.text);
          column += static_cast<int>(p.text.size());
        }
      },
      d.payload());
}

} // namespace

std::string LayoutRenderer::render(const DocPtr &doc, int maxLineLength,
                                   int indentSpaces) const {
  std::string out;
  if (!doc) {
    return out;
  }
  int column = 0;
  // Top-level always renders in break mode so that any unguarded
  // Line at the root produces an actual newline. Groups in the tree
  // re-enable flat mode where they fit.
  renderInto(out, *doc, maxLineLength, indentSpaces, /*indent=*/0, column,
             /*flatMode=*/false);
  return out;
}

} // namespace nsl::fmt
