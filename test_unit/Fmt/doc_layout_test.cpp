// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// test_unit/Fmt/doc_layout_test.cpp
//
// TDD fixtures for the Wadler-Leijen Doc IR + LayoutRenderer pair
// per `specs/010-t2-formatter-v0/data-model.md` §4 + research §3.
//
// Tasks covered (`tasks.md`):
//   T021 — `TextConcatRender`: `Doc::concat({text("a"), text("b")})`
//          renders to `"ab"`.
//   T022 — `GroupRibbonFitting`: `Doc::group(concat({text("a"),
//          line(), text("b")}))` renders to `"a b"` at width 100
//          and `"a\nb"` at width 1.
//   T023 — `NestIndent`: `Doc::nest(4, concat({hardline(), text("x")}))`
//          renders with 4-space indent.

#include "Doc.h"
#include "LayoutRenderer.h"

#include "gtest/gtest.h"
#include <string>

using nsl::fmt::Doc;
using nsl::fmt::DocPtr;
using nsl::fmt::LayoutRenderer;

namespace {

// Conventional renderer args for tests:
//   maxLineLength = 100  (matches default Configuration)
//   indentSpaces  = 4    (matches default Configuration::Indent::Spaces4)
constexpr int kDefaultWidth = 100;
constexpr int kDefaultIndent = 4;

LayoutRenderer renderer;

// -----------------------------------------------------------------------------
// T021 — TextConcatRender
// -----------------------------------------------------------------------------
TEST(DocLayoutTest, TextConcatRender) {
  DocPtr d = Doc::concat({Doc::text("a"), Doc::text("b")});
  EXPECT_EQ(renderer.render(d, kDefaultWidth, kDefaultIndent), "ab");
}

// -----------------------------------------------------------------------------
// T022 — GroupRibbonFitting
// -----------------------------------------------------------------------------
TEST(DocLayoutTest, GroupRibbonFittingFitsFlat) {
  // At width 100 the inner trivially fits: "a b" (3 chars) <= 100.
  DocPtr d =
      Doc::group(Doc::concat({Doc::text("a"), Doc::line(), Doc::text("b")}));
  EXPECT_EQ(renderer.render(d, /*maxLineLength=*/100, kDefaultIndent), "a b");
}

TEST(DocLayoutTest, GroupRibbonFittingBreaks) {
  // At width 1 the flat layout "a b" (3 chars) does NOT fit; the
  // group falls back to break mode: "a\n<indent>b" — at indent 0
  // this is "a\nb".
  DocPtr d =
      Doc::group(Doc::concat({Doc::text("a"), Doc::line(), Doc::text("b")}));
  EXPECT_EQ(renderer.render(d, /*maxLineLength=*/1, kDefaultIndent), "a\nb");
}

// -----------------------------------------------------------------------------
// T023 — NestIndent
// -----------------------------------------------------------------------------
TEST(DocLayoutTest, NestIndentEmitsFourSpaces) {
  // Doc::nest(4, concat({hardline(), text("x")})) at indent=0:
  //   - hardline always breaks; the new line is indented at
  //     0 + 4 = 4 spaces.
  //   - "x" follows on the indented line.
  // Expected: "\n    x"
  DocPtr d = Doc::nest(4, Doc::concat({Doc::hardline(), Doc::text("x")}));
  EXPECT_EQ(renderer.render(d, kDefaultWidth, /*indentSpaces=*/1), "\n    x");
}

// -----------------------------------------------------------------------------
// Tab-mode (`indentSpaces == -1`) emits one literal `\t` per indent unit.
// Regression guard for PR-#18 CodeRabbit Major-#2: prior code emitted
// exactly one `\t` regardless of nest depth, collapsing all nested
// blocks to a single tab. The renderer now emits `columnIndent` tabs.
// -----------------------------------------------------------------------------
TEST(DocLayoutTest, TabModeOneTabPerNestLevel) {
  // nest(1, nest(1, hardline + "x")) at indentSpaces=-1 (tab mode):
  //   - depth 2 → emits two literal tabs after the hardline.
  DocPtr d = Doc::nest(
      1, Doc::nest(1, Doc::concat({Doc::hardline(), Doc::text("x")})));
  EXPECT_EQ(renderer.render(d, kDefaultWidth, /*indentSpaces=*/-1),
            "\n\t\tx");

  // Single level → one tab.
  DocPtr d1 = Doc::nest(1, Doc::concat({Doc::hardline(), Doc::text("x")}));
  EXPECT_EQ(renderer.render(d1, kDefaultWidth, /*indentSpaces=*/-1),
            "\n\tx");

  // Zero indent → no tab (no leading whitespace at column 0).
  DocPtr d0 = Doc::concat({Doc::hardline(), Doc::text("x")});
  EXPECT_EQ(renderer.render(d0, kDefaultWidth, /*indentSpaces=*/-1),
            "\nx");
}

// -----------------------------------------------------------------------------
// Bonus: hardline forces break inside Group regardless of width.
// -----------------------------------------------------------------------------
TEST(DocLayoutTest, HardlineForcesGroupToBreak) {
  // A hardline inside a group MUST break even if the flat layout
  // would otherwise fit.
  DocPtr d = Doc::group(
      Doc::concat({Doc::text("a"), Doc::hardline(), Doc::text("b")}));
  // At a width that easily fits "a b" flat (5), the hardline still
  // forces break mode.
  EXPECT_EQ(renderer.render(d, /*maxLineLength=*/100, /*indentSpaces=*/1),
            "a\nb");
}

// -----------------------------------------------------------------------------
// Bonus: Align uses the current column.
// -----------------------------------------------------------------------------
TEST(DocLayoutTest, AlignSnapsIndentToColumn) {
  // "abc" + align(concat(line, "x")) at width 1:
  //   - emit "abc" → column 3
  //   - align sets indent = 3
  //   - line in break mode emits "\n   "
  //   - then "x"
  // Expected: "abc\n   x"
  DocPtr d = Doc::concat({
      Doc::text("abc"),
      Doc::align(Doc::concat({Doc::hardline(), Doc::text("x")})),
  });
  EXPECT_EQ(renderer.render(d, /*maxLineLength=*/1, /*indentSpaces=*/1),
            "abc\n   x");
}

} // namespace
