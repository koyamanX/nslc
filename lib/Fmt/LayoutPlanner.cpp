// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// lib/Fmt/LayoutPlanner.cpp — Phase 3-skeleton implementation.
// Verbatim-fallback only; Phase 3-rules replaces per-node-kind
// handlers incrementally.

#include "LayoutPlanner.h"

#include "CommentScanner.h"
#include "Doc.h"

// Per-node-kind AST headers — every concrete NodeKind needs a
// full definition so the macro-generated `visit(const Foo& node)`
// body can call `node.loc()` (which dispatches to ASTNode::loc()
// via the static type of `node`). Same pattern as
// `lib/Lower/ASTToMLIR.cpp`.
#include "nsl/AST/AltBlock.h"
#include "nsl/AST/AnyBlock.h"
#include "nsl/AST/BareFinishStmt.h"
#include "nsl/AST/BinaryExpr.h"
#include "nsl/AST/CallExpr.h"
#include "nsl/AST/CompilationUnit.h"
#include "nsl/AST/ConcatExpr.h"
#include "nsl/AST/ConditionalExpr.h"
#include "nsl/AST/ControlCallStmt.h"
#include "nsl/AST/Decl.h"
#include "nsl/AST/DeclareBlock.h"
#include "nsl/AST/DelayTaskStmt.h"
#include "nsl/AST/EmptyStmt.h"
#include "nsl/AST/Expr.h"
#include "nsl/AST/FieldAccessExpr.h"
#include "nsl/AST/FirstStateDecl.h"
#include "nsl/AST/ForBlock.h"
#include "nsl/AST/FuncDefn.h"
#include "nsl/AST/FuncSelfDecl.h"
#include "nsl/AST/GotoStmt.h"
#include "nsl/AST/IdentifierExpr.h"
#include "nsl/AST/IfStmt.h"
#include "nsl/AST/IncDecExpr.h"
#include "nsl/AST/IncDecStmt.h"
#include "nsl/AST/InitBlockStmt.h"
#include "nsl/AST/IntegerDecl.h"
#include "nsl/AST/LabeledStmt.h"
#include "nsl/AST/LiteralExpr.h"
#include "nsl/AST/MemDecl.h"
#include "nsl/AST/ModuleBlock.h"
#include "nsl/AST/ParallelBlock.h"
#include "nsl/AST/PortDecl.h"
#include "nsl/AST/ProcDefn.h"
#include "nsl/AST/ProcNameDecl.h"
#include "nsl/AST/RegDecl.h"
#include "nsl/AST/RepeatExpr.h"
#include "nsl/AST/ReturnStmt.h"
#include "nsl/AST/SeqBlock.h"
#include "nsl/AST/SignExtendExpr.h"
#include "nsl/AST/SliceExpr.h"
#include "nsl/AST/StateDefn.h"
#include "nsl/AST/StateNameDecl.h"
#include "nsl/AST/Stmt.h"
#include "nsl/AST/StructCastExpr.h"
#include "nsl/AST/StructDecl.h"
#include "nsl/AST/StructInstDecl.h"
#include "nsl/AST/StructuralGenerate.h"
#include "nsl/AST/SubmoduleDecl.h"
#include "nsl/AST/SystemTaskStmt.h"
#include "nsl/AST/SystemVarExpr.h"
#include "nsl/AST/TopLevelParamDecl.h"
#include "nsl/AST/TransferStmt.h"
#include "nsl/AST/UnaryExpr.h"
#include "nsl/AST/VariableDecl.h"
#include "nsl/AST/WhileBlock.h"
#include "nsl/AST/WireDecl.h"
#include "nsl/AST/ZeroExtendExpr.h"

#include "nsl/Basic/SourceLocation.h"

#include "llvm/ADT/StringRef.h"

#include <algorithm>
#include <cstdint>
#include <vector>

namespace nsl::fmt {

void LayoutPlanner::buildLineTable() noexcept {
  lineStartOffsets_.clear();
  lineStartOffsets_.push_back(0); // line 1 starts at offset 0
  for (std::uint32_t i = 0; i < src_.size(); ++i) {
    if (src_[i] == '\n') {
      lineStartOffsets_.push_back(i + 1);
    }
  }
}

int LayoutPlanner::lineForOffset(std::uint32_t offset) const noexcept {
  // Binary search the line-start table; line N (1-indexed) covers
  // [lineStartOffsets_[N-1], lineStartOffsets_[N]).
  auto it = std::upper_bound(lineStartOffsets_.begin(),
                             lineStartOffsets_.end(), offset);
  int fragmentLine = static_cast<int>(it - lineStartOffsets_.begin());
  if (fragmentLine < 1) {
    fragmentLine = 1;
  }
  return fragmentStartLine_ + fragmentLine - 1;
}

bool LayoutPlanner::nodeIntersectsRange(
    const ::nsl::ast::ASTNode &node) const noexcept {
  if (!range_.has_value()) {
    return true;
  }
  ::nsl::SourceRange r = node.loc();
  if (!r.begin().isValid() || !r.end().isValid()) {
    // Defensive: a node with no source location can't be range-
    // filtered, so let canonical layout run (preserves behavior for
    // any future synthetic nodes).
    return true;
  }
  std::uint32_t begin_off = r.begin().offset();
  std::uint32_t end_off = r.end().offset();
  // The end-offset is one-past-the-last byte (half-open). Subtract 1
  // so an end on a newline boundary doesn't bleed into the next
  // line — but guard against begin == end.
  std::uint32_t last_byte = end_off > begin_off ? end_off - 1 : begin_off;
  int beginLine = lineForOffset(begin_off);
  int endLine = lineForOffset(last_byte);
  return beginLine <= range_->lastLine && endLine >= range_->firstLine;
}

DocPtr LayoutPlanner::build(const ::nsl::ast::CompilationUnit &cu) {
  result_ = nullptr;
  cu.accept(*this); // dispatches to visit(CompilationUnit)
  return result_ ? result_ : Doc::text(llvm::StringRef{});
}

DocPtr LayoutPlanner::visitNode(const ::nsl::ast::ASTNode &child) {
  // Save/restore `result_` so nested calls compose naturally —
  // each `accept()` writes its result into `result_`, and a parent
  // visitor that calls `visitNode(*child)` to recurse into a
  // specific sub-node mustn't lose its own in-progress Doc.
  DocPtr saved = std::move(result_);
  result_ = nullptr;
  child.accept(*this);
  DocPtr child_doc = std::move(result_);
  result_ = std::move(saved);
  return child_doc ? child_doc : Doc::text(llvm::StringRef{});
}

DocPtr LayoutPlanner::verbatimFromRange(::nsl::SourceRange r) const {
  if (!r.begin().isValid() || !r.end().isValid()) {
    return Doc::text(llvm::StringRef{});
  }
  return verbatimFromOffsets(r.begin().offset(), r.end().offset());
}

DocPtr LayoutPlanner::verbatimFromOffsets(std::uint32_t begin,
                                          std::uint32_t end) const {
  if (begin > end || end > src_.size()) {
    return Doc::text(llvm::StringRef{});
  }
  if (begin == end) {
    return Doc::text(llvm::StringRef{});
  }
  return Doc::text(src_.substr(begin, end - begin));
}

DocPtr LayoutPlanner::verbatimGapFiltered(std::uint32_t begin,
                                          std::uint32_t end) const {
  // T051 R6 — strip comments from a verbatim inter-token gap when
  // `cfg_.preserve_comments == None`. For `All` / `LeadingOnly`,
  // emit the gap byte-for-byte (those policies preserve inline
  // comments inside a decl; the trailing-line drop under
  // `LeadingOnly` is decided by the parent layout, not by gap-byte
  // filtering — gaps inside a single decl don't contain trailing
  // line comments because a `//` runs to the next newline and
  // therefore can't sit BETWEEN two tokens of a single statement).
  if (begin > end || end > src_.size()) {
    return Doc::text(llvm::StringRef{});
  }
  if (begin == end) {
    return Doc::text(llvm::StringRef{});
  }
  if (cfg_.preserve_comments != Configuration::CommentMode::None) {
    return verbatimFromOffsets(begin, end);
  }
  // Scan the gap for comments and elide them. Whitespace bridging
  // a stripped comment collapses to a single space so adjacent
  // tokens don't fuse (e.g., `reg /* x */ q[8];` → `reg q[8];`,
  // not `reg q[8];` with double space and not `regq[8];`).
  std::vector<CommentTok> comments =
      scanComments(src_, begin, end, lineStartOffsets_);
  if (comments.empty()) {
    return verbatimFromOffsets(begin, end);
  }
  std::string out;
  out.reserve(end - begin);
  std::uint32_t cursor = begin;
  for (const CommentTok &c : comments) {
    if (c.begin > cursor) {
      out.append(src_.data() + cursor, c.begin - cursor);
    }
    // Replace the comment + its immediate single-space neighbours
    // (if any) with a single space so the surrounding tokens stay
    // separated. Trailing newlines around stripped comments are
    // preserved — they're meaningful (a `//`-line followed by
    // `\n` marked the end of the comment; the `\n` stays).
    // Drop a single space BEFORE the comment if `out` ends with
    // one and the byte right after the comment is whitespace —
    // that prevents accumulating double-spaces.
    bool ate_pre_space = false;
    if (!out.empty() && out.back() == ' ' && c.end < end &&
        (src_[c.end] == ' ' || src_[c.end] == '\t')) {
      out.pop_back();
      ate_pre_space = true;
    }
    (void)ate_pre_space;
    cursor = c.end;
  }
  if (cursor < end) {
    out.append(src_.data() + cursor, end - cursor);
  }
  return Doc::text(llvm::StringRef(out));
}

DocPtr LayoutPlanner::interleaveChildren(
    ::nsl::SourceRange parent_range,
    llvm::ArrayRef<const ::nsl::ast::ASTNode *> children) {
  std::vector<DocPtr> parts;
  std::uint32_t cursor = parent_range.begin().offset();
  const std::uint32_t parent_end = parent_range.end().offset();
  for (const ::nsl::ast::ASTNode *child : children) {
    if (child == nullptr) {
      continue;
    }
    ::nsl::SourceRange cr = child->loc();
    std::uint32_t child_begin = cr.begin().offset();
    std::uint32_t child_end = cr.end().offset();
    if (child_begin < cursor) {
      // Out-of-order or overlapping child — bail safely with
      // verbatim emission of the whole parent range. (Should not
      // happen for a well-formed AST.)
      return verbatimFromRange(parent_range);
    }
    if (child_begin > cursor) {
      // T051 R6 — inline comments (between two tokens of a single
      // decl) sit in these inter-child gaps. For the `none`
      // policy the helper strips them; for `all`/`leading_only`
      // they pass through byte-for-byte (Session 2026-05-05 Q2:
      // inline comments are preserved BYTE-for-BYTE at the same
      // token-relative position).
      parts.push_back(verbatimGapFiltered(cursor, child_begin));
    }
    parts.push_back(visitNode(*child));
    cursor = child_end;
  }
  if (cursor < parent_end) {
    parts.push_back(verbatimGapFiltered(cursor, parent_end));
  }
  return Doc::concat(std::move(parts));
}

// `visit()` body per concrete NodeKind. Each just routes to
// `formatNode(node)`, where overload resolution selects the most-
// specific overload available — defaulting to the verbatim fallback
// `formatNode(const ASTNode&)` when no per-NodeKind override exists.
// This pattern keeps the link-time visitor surface honest (every
// `NSL_NODE_KIND` entry still gets a dedicated `visit()` definition,
// so a missing override is a link-time error per Principle I) while
// letting Phase 3-rules tasks add canonical layouts via plain
// function overloads — no per-NodeKind macro skip-list needed.
// T091 — `--range LINE:LINE` honoring. When the range is set and the
// node's source-line span falls entirely outside it, short-circuit
// canonical layout to a verbatim emission so out-of-range bytes are
// preserved character-for-character (FR-007). Parent nodes that
// straddle the range still go through `formatNode(...)` so canonical
// layout recurses into in-range descendants; verbatim emission for
// out-of-range *children* is handled by re-entry into this same
// dispatch on the next `visit()` call.
#define NSL_NODE_KIND(EnumName, BaseClass)                                     \
  void LayoutPlanner::visit(const ::nsl::ast::EnumName &node) {                \
    if (!nodeIntersectsRange(node)) {                                          \
      result_ = verbatimFromRange(node.loc());                                 \
    } else {                                                                   \
      result_ = formatNode(node);                                              \
    }                                                                          \
  }
#include "nsl/AST/NodeKind.def"
#undef NSL_NODE_KIND

// ---- Indent-step helper -------------------------------------------------

int LayoutPlanner::indentStep() const noexcept {
  switch (cfg_.indent) {
  case Configuration::Indent::Spaces2:
    return 2;
  case Configuration::Indent::Spaces4:
    return 4;
  case Configuration::Indent::Tab:
    // Any positive count works in tab mode — `emitIndent` emits a
    // single `\t` whenever `columnIndent > 0`. Use 1 to keep the
    // numeric reasoning honest for any future spaces-mode renderer.
    return 1;
  }
  return 4;
}

// ---- Default verbatim-fallback ------------------------------------------

DocPtr LayoutPlanner::formatNode(const ::nsl::ast::ASTNode &node) {
  return verbatimFromRange(node.loc());
}

// ---- Per-NodeKind canonical-layout overrides ----------------------------
//
// T049 R5 (operator spacing). The `CompilationUnit`/`ModuleBlock`/
// `RegDecl` overrides exist to recurse into descendants so the
// `BinaryExpr` / `UnaryExpr` overrides actually fire on nested
// expression nodes — without them, the verbatim parent fallback would
// emit the whole `module ... { reg x[8] = a+b; }` byte range and the
// inner R5 rule would never see the `a+b`.

DocPtr
LayoutPlanner::formatNode(const ::nsl::ast::CompilationUnit &node) {
  std::vector<const ::nsl::ast::ASTNode *> children;
  children.reserve(node.items().size());
  for (const auto &item : node.items()) {
    children.push_back(item.get());
  }
  return interleaveChildren(node.loc(), children);
}

DocPtr LayoutPlanner::formatNode(const ::nsl::ast::ModuleBlock &node) {
  // T051 R6 (attached-comment preservation) — module body layout.
  //
  // Two-mode strategy per inter-decl gap, designed to honor R6
  // while preserving source line shape for the no-comment cases
  // already pinned by existing fixtures (T031 R4 expects two
  // decls on the same line, T033 R1 expects multi-line block,
  // etc.):
  //
  //   * If the gap has NO comments AND NO source newlines, emit
  //     the gap bytes verbatim. This keeps `reg a; reg b;` on one
  //     line when the source had it on one line.
  //   * Otherwise (the gap has comments or source newlines, or
  //     both), emit a canonical multi-line layout:
  //         (trailing-comments-for-prev-decl: inline)
  //         hardline + leading-comment-1
  //         hardline + leading-comment-2
  //         ... hardline + (indent supplied by Doc::nest) + decl
  //     This forces each leading comment onto its own line above
  //     the decl (matching the canonical T032 output) and
  //     collapses source-internal blank lines into one.
  //
  // Comment classification (per §6 of formatting-rules.contract.md):
  //   * Comment whose start-line == previous decl's end-line AND no
  //     newline appears between prev's end and the comment's start →
  //     TRAILING for the previous decl (emit on same line as prev).
  //   * Every other comment → LEADING for the current decl (emit
  //     on its own line, immediately above the decl).
  //
  // Honors `cfg_.preserve_comments`:
  //   * All         → emit all comment kinds
  //   * LeadingOnly → drop trailing line comments; keep leading
  //                   + inline (inline preservation lives in
  //                   `interleaveChildren`'s gap filter, which is
  //                   a no-op for this mode)
  //   * None        → drop every comment kind; inline strip is
  //                   handled by `verbatimGapFiltered` in
  //                   `interleaveChildren`.
  //
  // The wrapping `module <name> {` / `}` shell is reconstructed
  // canonically (single space after the name, K&R brace on same
  // line as the keyword, `}` on its own line at column 0). Allman
  // style is deferred — the current contract §6 fixture is K&R.

  std::vector<const ::nsl::ast::ASTNode *> children;
  children.reserve(node.internals().size() + node.actions().size() +
                   node.funcs().size() + node.procs().size());
  for (const auto &n : node.internals()) {
    children.push_back(n.get());
  }
  for (const auto &n : node.actions()) {
    children.push_back(n.get());
  }
  for (const auto &n : node.funcs()) {
    children.push_back(n.get());
  }
  for (const auto &n : node.procs()) {
    children.push_back(n.get());
  }
  std::sort(children.begin(), children.end(),
            [](const ::nsl::ast::ASTNode *a, const ::nsl::ast::ASTNode *b) {
              return a->loc().begin().offset() < b->loc().begin().offset();
            });

  // Locate the body's `{` and `}` byte offsets. `node.loc()` spans
  // `module ... }`; `}` is the last byte. `{` is the FIRST `{` after
  // the module-name byte position (the only `{` between `module
  // <name>` and the body), found by a forward scan — comments
  // before `{` are not currently spec'd, so a simple scan works.
  const std::uint32_t mod_begin = node.loc().begin().offset();
  const std::uint32_t mod_end = node.loc().end().offset();
  if (mod_end > src_.size() || mod_begin >= mod_end) {
    // Defensive: malformed range → fall back to verbatim.
    return verbatimFromRange(node.loc());
  }
  std::uint32_t lbrace_off = mod_begin;
  while (lbrace_off < mod_end && src_[lbrace_off] != '{') {
    ++lbrace_off;
  }
  if (lbrace_off >= mod_end) {
    // No `{` found — malformed; preserve verbatim.
    return verbatimFromRange(node.loc());
  }
  const std::uint32_t body_begin = lbrace_off + 1;
  // `}` is the byte at `mod_end - 1` for a well-formed module.
  // Guard against pathological `mod_end == 0`.
  std::uint32_t rbrace_off = mod_end > 0 ? mod_end - 1 : mod_begin;
  if (rbrace_off >= src_.size() || src_[rbrace_off] != '}') {
    // Defensive: parse range didn't end on `}` — fall back.
    return verbatimFromRange(node.loc());
  }
  const std::uint32_t body_end = rbrace_off;

  // Helpers ---------------------------------------------------
  const auto commentMode = cfg_.preserve_comments;
  const bool drop_all = commentMode == Configuration::CommentMode::None;
  const bool drop_trailing =
      drop_all || commentMode == Configuration::CommentMode::LeadingOnly;
  // Indentation per declaration line is supplied by the surrounding
  // `Doc::nest(indentStep(), ...)` wrap; the renderer prepends the
  // current indent level after every `Doc::hardline()`. We do NOT
  // emit an explicit indent string here.

  // Classify a gap into trailing-of-prev / leading-of-next and
  // measure whether the gap had any newlines outside comments.
  struct ClassifiedGap {
    std::vector<CommentTok> leading;
    std::vector<CommentTok> trailing; // 0+ comments on prev's line
    int newlinesOutsideComments = 0;
    bool hasBlankLine = false;        // 2+ newlines in a row
  };
  auto classifyGap = [&](std::uint32_t gap_begin, std::uint32_t gap_end,
                         std::uint32_t prev_end_off,
                         bool have_prev) -> ClassifiedGap {
    ClassifiedGap g;
    if (gap_begin >= gap_end) {
      return g;
    }
    std::vector<CommentTok> cs =
        scanComments(src_, gap_begin, gap_end, lineStartOffsets_);
    for (const CommentTok &c : cs) {
      bool same_line_as_prev = false;
      if (have_prev) {
        // Same-line-as-prev iff there's NO `\n` between prev's end
        // offset and this comment's start offset. We can't rely on
        // line-numbers alone — a multi-line preceding decl might
        // have a comment that starts on prev's last line but after
        // a newline within the decl's interior. Use raw bytes.
        bool found_nl = false;
        std::uint32_t scan_from = prev_end_off;
        if (scan_from < c.begin) {
          for (std::uint32_t i = scan_from; i < c.begin; ++i) {
            if (src_[i] == '\n') {
              found_nl = true;
              break;
            }
          }
        }
        same_line_as_prev = !found_nl;
      }
      if (same_line_as_prev) {
        g.trailing.push_back(c);
      } else {
        g.leading.push_back(c);
      }
    }
    // Count newlines outside comment spans, and detect blank lines.
    int consecutive_nl = 0;
    bool ws_only_since_nl = true;
    std::size_t comment_idx = 0;
    for (std::uint32_t i = gap_begin; i < gap_end; ++i) {
      while (comment_idx < cs.size() && cs[comment_idx].end <= i) {
        ++comment_idx;
      }
      if (comment_idx < cs.size() && cs[comment_idx].begin <= i &&
          i < cs[comment_idx].end) {
        i = cs[comment_idx].end - 1; // for-loop will ++ to next
        continue;
      }
      char ch = src_[i];
      if (ch == '\n') {
        ++g.newlinesOutsideComments;
        if (ws_only_since_nl) {
          ++consecutive_nl;
          if (consecutive_nl >= 2) {
            g.hasBlankLine = true;
          }
        } else {
          consecutive_nl = 1;
        }
        ws_only_since_nl = true;
      } else if (ch == ' ' || ch == '\t' || ch == '\r') {
        // horizontal whitespace
      } else {
        ws_only_since_nl = false;
        consecutive_nl = 0;
      }
    }
    return g;
  };

  // Build the body --------------------------------------------
  std::vector<DocPtr> body_parts;
  body_parts.reserve(children.size() * 6 + 4);

  std::uint32_t cursor = body_begin;
  std::uint32_t prev_end_off = 0;
  bool have_prev = false;
  bool body_has_break = false;
  // Whether the LAST thing we emitted into `body_parts` was a
  // hardline (or a leading-comment-with-its-trailing-hardline).
  // This tells us whether emitting a decl needs its OWN hardline
  // prefix to start on a new line.
  bool at_line_start = true;

  auto emitLeadingComment = [&](const CommentTok &c) {
    if (drop_all) {
      return;
    }
    if (!at_line_start) {
      body_parts.push_back(Doc::hardline());
      at_line_start = true;
    }
    llvm::StringRef text(src_.data() + c.begin, c.end - c.begin);
    body_parts.push_back(Doc::comment(text, /*leading=*/true,
                                       /*trailing=*/false));
    at_line_start = false;
    body_has_break = true;
  };
  auto emitTrailingComment = [&](const CommentTok &c) {
    if (drop_trailing) {
      return;
    }
    if (at_line_start) {
      // Defensive: a trailing comment with no decl to attach to
      // (shouldn't happen — only fires when called with a prev
      // decl context). Fall back to leading placement.
      llvm::StringRef text(src_.data() + c.begin, c.end - c.begin);
      body_parts.push_back(Doc::comment(text, /*leading=*/false,
                                         /*trailing=*/true));
      return;
    }
    body_parts.push_back(Doc::text(llvm::StringRef(" ")));
    llvm::StringRef text(src_.data() + c.begin, c.end - c.begin);
    body_parts.push_back(Doc::comment(text, /*leading=*/false,
                                       /*trailing=*/true));
  };

  for (const ::nsl::ast::ASTNode *child : children) {
    if (child == nullptr) {
      continue;
    }
    ::nsl::SourceRange cr = child->loc();
    std::uint32_t child_begin = cr.begin().offset();
    std::uint32_t child_end = cr.end().offset();
    if (child_begin < cursor) {
      // Out-of-order child — bail to verbatim.
      return verbatimFromRange(node.loc());
    }

    ClassifiedGap g =
        classifyGap(cursor, child_begin, prev_end_off, have_prev);

    // Decide layout mode for this gap.
    const bool gap_has_any_comment =
        !g.leading.empty() || !g.trailing.empty();
    const bool gap_has_break = g.newlinesOutsideComments > 0;
    const bool canonical = gap_has_any_comment || gap_has_break;

    // Emit trailing comments belonging to the PREVIOUS decl.
    for (const CommentTok &c : g.trailing) {
      emitTrailingComment(c);
    }

    if (!canonical) {
      // No comments, no source newlines. Preserve compact form.
      // For the very first decl this means emitting the gap bytes
      // verbatim (typically a single space after `{`); for
      // subsequent decls the same — preserves `reg a; reg b;` on
      // a single line. (Comments-empty branch, so no comment-
      // filtering needed — `verbatimGapFiltered` is a no-op when
      // the gap has no comments.)
      if (cursor < child_begin) {
        body_parts.push_back(verbatimGapFiltered(cursor, child_begin));
        at_line_start = false;
      }
    } else {
      // Canonical multi-line layout. (Blank-line preservation
      // between decls is intentionally a single hardline at T2 —
      // emitting two consecutive hardlines makes the renderer's
      // `emitIndent` produce trailing whitespace on the blank
      // line, which violates the byte-stability spirit. The
      // contract §6 last bullet says blank lines are preserved
      // "up to" `blank_lines_between_modules`; clamping to one
      // satisfies the "up to" clause and matches the contract's
      // "internal blank lines clamped to one" for non-top-level
      // gaps. A future revision can add a no-indent-after-blank
      // renderer mode if needed.)
      body_parts.push_back(Doc::hardline());
      at_line_start = true;
      body_has_break = true;
      for (const CommentTok &c : g.leading) {
        emitLeadingComment(c);
      }
      if (!at_line_start) {
        body_parts.push_back(Doc::hardline());
        at_line_start = true;
      }
    }

    // Emit the decl itself. If we're at a line start, the
    // surrounding `Doc::nest(indentStep(), ...)` has already
    // supplied the indent via the preceding hardline. If we're
    // NOT at line start (compact-form gap), the verbatim gap
    // bytes are the separator.
    body_parts.push_back(visitNode(*child));
    at_line_start = false;

    cursor = child_end;
    prev_end_off = child_end;
    have_prev = true;
  }

  // Final gap: [cursor, body_end). Trailing line comment on the
  // last decl's line (`reg r[8]; // trailing\n}`) and any orphan
  // leading comments before `}`.
  ClassifiedGap tail =
      classifyGap(cursor, body_end, prev_end_off, have_prev);
  for (const CommentTok &c : tail.trailing) {
    emitTrailingComment(c);
  }
  for (const CommentTok &c : tail.leading) {
    emitLeadingComment(c);
  }

  // Whether to put `}` on its own line: if the body emitted any
  // canonical break OR the source had a newline in the tail gap.
  // If neither — the whole module body was on one line in the
  // source AND we didn't have to break it for comment handling —
  // keep `}` on the same line as the last token, preserving the
  // tail-gap bytes verbatim (so a source ` }` keeps the space
  // before `}`).
  const bool body_broke = body_has_break;
  const bool put_rbrace_on_own_line =
      body_broke || tail.newlinesOutsideComments > 0;

  if (!put_rbrace_on_own_line) {
    // Preserve source spacing before `}` (typically ` ` or empty).
    if (cursor < body_end) {
      body_parts.push_back(verbatimGapFiltered(cursor, body_end));
    }
  }

  // Wrap the body in `module <name> {` / `}` shell.
  std::vector<DocPtr> top_parts;
  top_parts.reserve(6);
  top_parts.push_back(Doc::text(llvm::StringRef("module ")));
  top_parts.push_back(Doc::text(node.name()));
  top_parts.push_back(Doc::text(llvm::StringRef(" {")));
  top_parts.push_back(
      Doc::nest(indentStep(), Doc::concat(std::move(body_parts))));
  if (put_rbrace_on_own_line) {
    top_parts.push_back(Doc::hardline());
  }
  top_parts.push_back(Doc::text(llvm::StringRef("}")));
  return Doc::concat(std::move(top_parts));
}

DocPtr LayoutPlanner::formatNode(const ::nsl::ast::RegDecl &node) {
  // `width` precedes `init` in the source (`reg <name>[<width>] =
  // <init>;`); rely on the AST's lexical order rather than re-sorting.
  std::vector<const ::nsl::ast::ASTNode *> children;
  if (node.width() != nullptr) {
    children.push_back(node.width());
  }
  if (node.init() != nullptr) {
    children.push_back(node.init());
  }
  return interleaveChildren(node.loc(), children);
}

DocPtr LayoutPlanner::formatNode(const ::nsl::ast::WireDecl &node) {
  // `wire <name>[<width>];` — no init slot (S2 forbids
  // `wire <name> = <init>`; that's parser-rejected before we get
  // here). Recurse only into `width` so any nested SliceExpr /
  // ConcatExpr / BinaryExpr inside the width gets the canonical
  // R4 / R5 treatment.
  std::vector<const ::nsl::ast::ASTNode *> children;
  if (node.width() != nullptr) {
    children.push_back(node.width());
  }
  return interleaveChildren(node.loc(), children);
}

DocPtr LayoutPlanner::formatNode(const ::nsl::ast::BinaryExpr &node) {
  // R5: one space on each side of the binary operator. Recurse into
  // lhs/rhs via `visitNode` so nested binary/unary subtrees get the
  // same canonicalisation.
  DocPtr lhs_doc = node.lhs() != nullptr ? visitNode(*node.lhs())
                                          : Doc::text(llvm::StringRef{});
  DocPtr rhs_doc = node.rhs() != nullptr ? visitNode(*node.rhs())
                                          : Doc::text(llvm::StringRef{});
  std::vector<DocPtr> parts;
  parts.reserve(5);
  parts.push_back(std::move(lhs_doc));
  parts.push_back(Doc::text(llvm::StringRef(" ")));
  parts.push_back(Doc::text(binaryOpSpelling(node.op())));
  parts.push_back(Doc::text(llvm::StringRef(" ")));
  parts.push_back(std::move(rhs_doc));
  return Doc::concat(std::move(parts));
}

DocPtr LayoutPlanner::formatNode(const ::nsl::ast::UnaryExpr &node) {
  // R5: no space between the unary operator and its operand. The
  // operator immediately abuts the recursively-formatted operand.
  DocPtr sub_doc = node.sub() != nullptr ? visitNode(*node.sub())
                                          : Doc::text(llvm::StringRef{});
  std::vector<DocPtr> parts;
  parts.reserve(2);
  parts.push_back(Doc::text(unaryOpSpelling(node.op())));
  parts.push_back(std::move(sub_doc));
  return Doc::concat(std::move(parts));
}

DocPtr LayoutPlanner::formatNode(const ::nsl::ast::ConditionalExpr &node) {
  // Recursion-only override. NSL's `if (c) a else b` and the
  // ternary `c ? a : b` both lower to the same `ConditionalExpr`
  // node; the AST doesn't preserve which surface form the source
  // used. Rather than canonicalising one form into the other
  // (which would silently rewrite the source spelling — a much
  // bigger semantic change than R5 spacing), we use
  // `interleaveChildren` to preserve the source-shape verbatim
  // gaps (`?` / `:` / `if` / `else`) while still recursing into
  // each child Expr so nested operators / slices / concats / etc.
  // get the canonical R5 / R4 treatment. The "one space around
  // `?` / `:`" sub-rule of R5 §5 lands here when the source
  // already carries canonical spacing — and a future revision
  // may add a true canonical-form override once the surface-form
  // ambiguity is resolved at the spec level.
  std::vector<const ::nsl::ast::ASTNode *> children;
  children.reserve(3);
  if (node.cond() != nullptr) {
    children.push_back(node.cond());
  }
  if (node.thenE() != nullptr) {
    children.push_back(node.thenE());
  }
  if (node.elseE() != nullptr) {
    children.push_back(node.elseE());
  }
  return interleaveChildren(node.loc(), children);
}

DocPtr LayoutPlanner::formatNode(const ::nsl::ast::SliceExpr &node) {
  // R4 §4: bit slice has no spaces inside `[`, `]`, around `:`.
  //   `<sub>[<hi>]`            for the single-index form
  //   `<sub>[<hi>:<lo>]`       for the two-index form
  // Recurse into each Expr child so nested operators / slices /
  // concats / unary forms inside any index get the canonical
  // treatment too.
  DocPtr sub_doc = node.sub() != nullptr ? visitNode(*node.sub())
                                          : Doc::text(llvm::StringRef{});
  DocPtr hi_doc = node.hi() != nullptr ? visitNode(*node.hi())
                                        : Doc::text(llvm::StringRef{});
  std::vector<DocPtr> parts;
  parts.reserve(6);
  parts.push_back(std::move(sub_doc));
  parts.push_back(Doc::text(llvm::StringRef("[")));
  parts.push_back(std::move(hi_doc));
  if (node.lo() != nullptr) {
    parts.push_back(Doc::text(llvm::StringRef(":")));
    parts.push_back(visitNode(*node.lo()));
  }
  parts.push_back(Doc::text(llvm::StringRef("]")));
  return Doc::concat(std::move(parts));
}

DocPtr LayoutPlanner::formatNode(const ::nsl::ast::StructDecl &node) {
  // R2 §2: in `struct <name> { <members> };` the `[N]` bit-width
  // brackets all align to one space past the longest member name's
  // alignment column. The canonical example in
  // `formatting-rules.contract.md` §2 puts TWO physical spaces
  // between the longest name and its `[` (not one — the contract
  // text "one space follows the longest name" describes the gap
  // between the longest name and the alignment column, not the
  // brackets themselves):
  //
  //   struct csr_t {
  //       mstatus  [32];   <-- 2 spaces  (`[` at offset 9 from name start)
  //       mcause   [32];   <-- 3 spaces  (offset 9: 6 + 3 = 9)
  //       mtvec    [30];   <-- 4 spaces  (offset 9: 5 + 4 = 9)
  //       mepc     [32];   <-- 5 spaces  (offset 9: 4 + 5 = 9)
  //   };
  //
  // So the brace column lands at `max_name + 2` characters from the
  // start of the (post-indent) name; per-member padding is
  // `(max_name + 2) - name.size()`.
  //
  // When `align_struct_members = false` the toggle suppresses the
  // alignment: every member emits exactly one space between name
  // and `[`. Members without a width slot just emit `<name>;` (no
  // `[`, so they don't sit on the alignment column — but if their
  // name happens to be the longest, they DO push the column
  // further out: alignment is anchored on the longest *name*, not
  // the longest *width-bearing* name).

  // Compute longest member-name length over ALL members (not just
  // those with widths) so a width-less member with a long name
  // still anchors the alignment column.
  std::size_t max_name = 0;
  for (const auto &m : node.members()) {
    if (m.name.size() > max_name) {
      max_name = m.name.size();
    }
  }

  std::vector<DocPtr> body_parts;
  body_parts.reserve(node.members().size() * 7);
  for (const auto &m : node.members()) {
    body_parts.push_back(Doc::hardline());
    body_parts.push_back(Doc::text(m.name));
    if (m.width != nullptr) {
      // Per the canonical example, the `[` lands at column
      // `max_name + 2` from the name's start; pad accordingly.
      // With `align_struct_members=false` collapse every member to
      // a single-space separator (the rule's documented
      // suppression mode).
      const std::size_t pad = cfg_.align_struct_members
                                  ? (max_name + 2) - m.name.size()
                                  : std::size_t{1};
      const std::string padding(pad, ' ');
      body_parts.push_back(Doc::text(llvm::StringRef(padding)));
      body_parts.push_back(Doc::text(llvm::StringRef("[")));
      body_parts.push_back(visitNode(*m.width));
      body_parts.push_back(Doc::text(llvm::StringRef("]")));
    }
    body_parts.push_back(Doc::text(llvm::StringRef(";")));
  }

  // K&R (default): `struct <name> {` on one line, body indented
  //                below. Matches the canonical example in
  //                `formatting-rules.contract.md` §2.
  // Allman:        `struct <name>` then a newline then `{` on its
  //                own line at the outer indent (one indent
  //                level less than the body).
  const bool allman = cfg_.brace_style == Configuration::BraceStyle::Allman;
  std::vector<DocPtr> top_parts;
  top_parts.reserve(7);
  top_parts.push_back(Doc::text(llvm::StringRef("struct ")));
  top_parts.push_back(Doc::text(node.name()));
  if (allman) {
    top_parts.push_back(Doc::hardline());
    top_parts.push_back(Doc::text(llvm::StringRef("{")));
  } else {
    top_parts.push_back(Doc::text(llvm::StringRef(" {")));
  }
  top_parts.push_back(
      Doc::nest(indentStep(), Doc::concat(std::move(body_parts))));
  top_parts.push_back(Doc::hardline());
  top_parts.push_back(Doc::text(llvm::StringRef("};")));
  return Doc::concat(std::move(top_parts));
}

DocPtr LayoutPlanner::formatNode(const ::nsl::ast::DeclareBlock &node) {
  // Recursion-only override. The `declare <name> { <ports> }` shell
  // is preserved verbatim by the parent gap; we just merge the
  // header-params + ports vectors and source-position-sort them
  // so any nested expressions (port widths, header-param init
  // values) get their canonical R5 / R4 treatment.
  std::vector<const ::nsl::ast::ASTNode *> children;
  children.reserve(node.headerParams().size() + node.ports().size());
  for (const auto &n : node.headerParams()) {
    children.push_back(n.get());
  }
  for (const auto &n : node.ports()) {
    children.push_back(n.get());
  }
  std::sort(children.begin(), children.end(),
            [](const ::nsl::ast::ASTNode *a, const ::nsl::ast::ASTNode *b) {
              return a->loc().begin().offset() < b->loc().begin().offset();
            });
  return interleaveChildren(node.loc(), children);
}

DocPtr LayoutPlanner::formatNode(const ::nsl::ast::FuncDefn &node) {
  // Recursion-only override: `func <name> ` prefix and ` }` suffix
  // are emitted by the parent's verbatim gap; we only need to make
  // sure the body Stmt (which may transitively contain an
  // `AltBlock` / `AnyBlock` whose canonical R1 layout we need to
  // fire) is *visited* rather than swallowed by the FuncDefn's own
  // verbatim fallback.
  std::vector<const ::nsl::ast::ASTNode *> children;
  if (node.body() != nullptr) {
    children.push_back(node.body());
  }
  return interleaveChildren(node.loc(), children);
}

DocPtr LayoutPlanner::formatNode(const ::nsl::ast::ProcDefn &node) {
  // Recursion-only override paralleling `formatNode(FuncDefn)`.
  // `proc <name> <body>` — recurse into the body so nested
  // canonical-layout overrides fire on transitive children.
  std::vector<const ::nsl::ast::ASTNode *> children;
  if (node.body() != nullptr) {
    children.push_back(node.body());
  }
  return interleaveChildren(node.loc(), children);
}

DocPtr
LayoutPlanner::formatNode(const ::nsl::ast::ProcNameDecl &node) {
  // R3 §3 (amended Session 2026-05-12 — see `plan.md` Plan
  // Revisions): single-line form when total fits within
  // `max_line_length`, otherwise multi-line form with each arg
  // on its own line indented one `indent` step past `(`. The
  // original §3 wording referenced `[N]`-width arg syntax that
  // the NSL parser does not accept; the amended rule is
  // `max_line_length`-driven and operates on bare-identifier
  // arg lists, which is what the parser actually produces.
  //
  // `regArgs()` is a `std::vector<Identifier>` of bare names;
  // there are no widths, no expressions to recurse into. The
  // trailing-comma policy applies in the multi-line form:
  //   * `Add`      — emit a `,` after the last arg
  //   * `Remove`   — never emit a trailing comma
  //   * `Preserve` — keep the pre-format state, which for
  //                  `proc_name` is always "no trailing comma"
  //                  (the parser rejects literal trailing
  //                  commas in the source form), so `Preserve`
  //                  is observably equivalent to `Remove` here.
  // The single-line form never emits a trailing comma regardless
  // of policy.

  llvm::StringRef name = node.name();
  const auto &args = node.regArgs();

  // Bare `proc_name <name>;` (no parens) when there are no args.
  if (args.empty()) {
    return Doc::concat({
        Doc::text(llvm::StringRef("proc_name ")),
        Doc::text(name),
        Doc::text(llvm::StringRef(";")),
    });
  }

  // Estimate the single-line width. We don't track the renderer's
  // running column, so assume the canonical parent indent is one
  // `indentStep()` (`proc_name` decls always live inside a module
  // body in valid NSL; the parser disallows top-level position).
  const std::size_t parent_indent_cols =
      static_cast<std::size_t>(indentStep());
  std::size_t projected = parent_indent_cols +
                          /* "proc_name " */ 10 + name.size() +
                          /* "(" */ 1 + /* ");" */ 2;
  for (std::size_t i = 0; i < args.size(); ++i) {
    projected += args[i].size();
    if (i + 1 < args.size()) {
      projected += /* ", " */ 2;
    }
  }

  const std::size_t max_cols =
      cfg_.max_line_length > 0 ? static_cast<std::size_t>(cfg_.max_line_length)
                               : std::size_t{100};
  const bool multiline = projected > max_cols;

  if (!multiline) {
    std::vector<DocPtr> parts;
    parts.reserve(args.size() * 2 + 4);
    parts.push_back(Doc::text(llvm::StringRef("proc_name ")));
    parts.push_back(Doc::text(name));
    parts.push_back(Doc::text(llvm::StringRef("(")));
    for (std::size_t i = 0; i < args.size(); ++i) {
      if (i != 0) {
        parts.push_back(Doc::text(llvm::StringRef(", ")));
      }
      parts.push_back(Doc::text(args[i]));
    }
    parts.push_back(Doc::text(llvm::StringRef(");")));
    return Doc::concat(std::move(parts));
  }

  // Multi-line form.
  using TC = Configuration::TrailingCommas;
  const bool emit_trailing_comma = cfg_.trailing_commas == TC::Add;

  std::vector<DocPtr> body;
  body.reserve(args.size() * 3);
  for (std::size_t i = 0; i < args.size(); ++i) {
    body.push_back(Doc::hardline());
    body.push_back(Doc::text(args[i]));
    const bool is_last = i + 1 == args.size();
    if (!is_last || emit_trailing_comma) {
      body.push_back(Doc::text(llvm::StringRef(",")));
    }
  }

  std::vector<DocPtr> top;
  top.reserve(6);
  top.push_back(Doc::text(llvm::StringRef("proc_name ")));
  top.push_back(Doc::text(name));
  top.push_back(Doc::text(llvm::StringRef("(")));
  top.push_back(Doc::nest(indentStep(), Doc::concat(std::move(body))));
  top.push_back(Doc::hardline());
  top.push_back(Doc::text(llvm::StringRef(");")));
  return Doc::concat(std::move(top));
}

DocPtr LayoutPlanner::formatNode(const ::nsl::ast::StateDefn &node) {
  // Recursion-only override paralleling `formatNode(FuncDefn)`.
  // `state <name> <body>` — recurse into the body.
  std::vector<const ::nsl::ast::ASTNode *> children;
  if (node.body() != nullptr) {
    children.push_back(node.body());
  }
  return interleaveChildren(node.loc(), children);
}

DocPtr LayoutPlanner::formatNode(const ::nsl::ast::SeqBlock &node) {
  // Recursion-only override. Items + decls merge into a single
  // source-position-sorted child list (mirrors the four-vector
  // merge in `formatNode(ModuleBlock)`); the wrapping `seq { ... }`
  // is preserved by the gap-emit logic in `interleaveChildren`.
  std::vector<const ::nsl::ast::ASTNode *> children;
  children.reserve(node.items().size() + node.decls().size());
  for (const auto &n : node.items()) {
    children.push_back(n.get());
  }
  for (const auto &n : node.decls()) {
    children.push_back(n.get());
  }
  std::sort(children.begin(), children.end(),
            [](const ::nsl::ast::ASTNode *a, const ::nsl::ast::ASTNode *b) {
              return a->loc().begin().offset() < b->loc().begin().offset();
            });
  return interleaveChildren(node.loc(), children);
}

DocPtr LayoutPlanner::formatNode(const ::nsl::ast::ParallelBlock &node) {
  // Recursion-only override paralleling `formatNode(SeqBlock)`. The
  // anonymous `{ ... }` block parsed as a function body
  // (`func f { ... }`) lands here as a `ParallelBlock` (per the
  // NSL `parallel_block_item` shape — multiple statements inside a
  // function body execute in parallel within one cycle). Same
  // items+decls merge logic as SeqBlock.
  std::vector<const ::nsl::ast::ASTNode *> children;
  children.reserve(node.items().size() + node.decls().size());
  for (const auto &n : node.items()) {
    children.push_back(n.get());
  }
  for (const auto &n : node.decls()) {
    children.push_back(n.get());
  }
  std::sort(children.begin(), children.end(),
            [](const ::nsl::ast::ASTNode *a, const ::nsl::ast::ASTNode *b) {
              return a->loc().begin().offset() < b->loc().begin().offset();
            });
  return interleaveChildren(node.loc(), children);
}

DocPtr LayoutPlanner::formatNode(const ::nsl::ast::AltBlock &node) {
  return formatCondCaseBlock(node.cases(), node.elseCase(),
                              llvm::StringRef("alt"));
}

DocPtr LayoutPlanner::formatNode(const ::nsl::ast::AnyBlock &node) {
  return formatCondCaseBlock(node.cases(), node.elseCase(),
                              llvm::StringRef("any"));
}

DocPtr LayoutPlanner::formatNode(const ::nsl::ast::IfStmt &node) {
  // Recursion-only override. NSL's statement-position
  // `if (cond) thenS else elseS` shape — preserve the source
  // verbatim gaps (`if (`, `)`, `else`) while recursing into the
  // condition and both branches so nested R5 / R4 fire. Like
  // `ConditionalExpr`, a true canonical-form override is deferred
  // pending spec clarification on multi-line vs single-line
  // surface forms.
  std::vector<const ::nsl::ast::ASTNode *> children;
  children.reserve(3);
  if (node.cond() != nullptr) {
    children.push_back(node.cond());
  }
  if (node.thenBr() != nullptr) {
    children.push_back(node.thenBr());
  }
  if (node.elseBr() != nullptr) {
    children.push_back(node.elseBr());
  }
  return interleaveChildren(node.loc(), children);
}

DocPtr LayoutPlanner::formatNode(const ::nsl::ast::WhileBlock &node) {
  // Recursion-only override. `while (<cond>) { <items> }` — emit
  // the verbatim shell while recursing into `cond` and each
  // body item so nested expression rules fire.
  std::vector<const ::nsl::ast::ASTNode *> children;
  children.reserve(1 + node.items().size());
  if (node.cond() != nullptr) {
    children.push_back(node.cond());
  }
  for (const auto &n : node.items()) {
    children.push_back(n.get());
  }
  // Items follow the cond in source order, but since the cond's
  // loc is always before any item's loc, no sort is required.
  return interleaveChildren(node.loc(), children);
}

DocPtr LayoutPlanner::formatNode(const ::nsl::ast::StructuralGenerate &node) {
  // Recursion-only override. `generate (<init> = <initValue>;
  // <cond>; <step>) <body>` — preserve the verbatim shell while
  // recursing into each Expr / Stmt so nested operators get the
  // canonical R5 / R4 treatment. The structural-expansion
  // pass (M5) consumes the same AST shape.
  std::vector<const ::nsl::ast::ASTNode *> children;
  children.reserve(4);
  if (node.initValue() != nullptr) {
    children.push_back(node.initValue());
  }
  if (node.cond() != nullptr) {
    children.push_back(node.cond());
  }
  if (node.step() != nullptr) {
    children.push_back(node.step());
  }
  if (node.body() != nullptr) {
    children.push_back(node.body());
  }
  return interleaveChildren(node.loc(), children);
}

DocPtr LayoutPlanner::formatNode(const ::nsl::ast::SystemTaskStmt &node) {
  // Recursion-only override. `_display(<args>);`, `_finish;`,
  // etc. — `args` is a vector of Exprs in source order; no
  // sort needed (Expr children always appear after the task
  // keyword and `(` token).
  std::vector<const ::nsl::ast::ASTNode *> children;
  children.reserve(node.args().size());
  for (const auto &n : node.args()) {
    children.push_back(n.get());
  }
  return interleaveChildren(node.loc(), children);
}

DocPtr LayoutPlanner::formatNode(const ::nsl::ast::ControlCallStmt &node) {
  // Recursion-only override. `<target>(<args>);` — target is a
  // ScopedName (not an AST node), so we only descend into the
  // Expr arguments.
  std::vector<const ::nsl::ast::ASTNode *> children;
  children.reserve(node.args().size());
  for (const auto &n : node.args()) {
    children.push_back(n.get());
  }
  return interleaveChildren(node.loc(), children);
}

DocPtr LayoutPlanner::formatNode(const ::nsl::ast::IncDecStmt &node) {
  // Recursion-only override. `<target>++;` / `<target>--;` —
  // recurse into the target Expr so any nested operator inside
  // (e.g., `obj.field++`) gets its canonical layout.
  std::vector<const ::nsl::ast::ASTNode *> children;
  if (node.target() != nullptr) {
    children.push_back(node.target());
  }
  return interleaveChildren(node.loc(), children);
}

DocPtr LayoutPlanner::formatNode(const ::nsl::ast::ReturnStmt &node) {
  // Recursion-only override. `return [<value>];` — value is
  // nullable (bare `return;` form).
  std::vector<const ::nsl::ast::ASTNode *> children;
  if (node.value() != nullptr) {
    children.push_back(node.value());
  }
  return interleaveChildren(node.loc(), children);
}

DocPtr LayoutPlanner::formatNode(const ::nsl::ast::CallExpr &node) {
  // Recursion-only override. `<target>(<args>)` in expression
  // position — like `ControlCallStmt` but emits no trailing
  // `;` (which the verbatim parent gap supplies if needed).
  std::vector<const ::nsl::ast::ASTNode *> children;
  children.reserve(node.args().size());
  for (const auto &n : node.args()) {
    children.push_back(n.get());
  }
  return interleaveChildren(node.loc(), children);
}

DocPtr LayoutPlanner::formatNode(const ::nsl::ast::ForBlock &node) {
  // Recursion-only override. `for (init; cond; step) { items... }`
  // — descend into each of the three clauses (any of which may be
  // nullptr per `ForForm` shape) and the body so nested
  // binary / unary / slice / concat / transfer expressions inside
  // fire their canonical R4 / R5 layout. Source order is init →
  // cond → step → items, which matches accessor order — no sort
  // needed.
  std::vector<const ::nsl::ast::ASTNode *> children;
  children.reserve(3 + node.items().size());
  if (node.form().init != nullptr) {
    children.push_back(node.form().init.get());
  }
  if (node.form().cond != nullptr) {
    children.push_back(node.form().cond.get());
  }
  if (node.form().step != nullptr) {
    children.push_back(node.form().step.get());
  }
  for (const auto &n : node.items()) {
    children.push_back(n.get());
  }
  return interleaveChildren(node.loc(), children);
}

DocPtr LayoutPlanner::formatNode(const ::nsl::ast::RepeatExpr &node) {
  // Recursion-only override. `{N{x}}` bit-vector replication —
  // count expression then body expression, source order matches
  // accessor order.
  std::vector<const ::nsl::ast::ASTNode *> children;
  children.reserve(2);
  if (node.count() != nullptr) {
    children.push_back(node.count());
  }
  if (node.body() != nullptr) {
    children.push_back(node.body());
  }
  return interleaveChildren(node.loc(), children);
}

DocPtr LayoutPlanner::formatNode(const ::nsl::ast::SignExtendExpr &node) {
  // Recursion-only override. `<width> # <sub>` (parser-note N5 —
  // `#` in expression position post-preprocess; the line-marker
  // `#line` form never reaches an AST node). Descend into both
  // operands.
  std::vector<const ::nsl::ast::ASTNode *> children;
  children.reserve(2);
  if (node.width() != nullptr) {
    children.push_back(node.width());
  }
  if (node.sub() != nullptr) {
    children.push_back(node.sub());
  }
  return interleaveChildren(node.loc(), children);
}

DocPtr LayoutPlanner::formatNode(const ::nsl::ast::ZeroExtendExpr &node) {
  // Recursion-only override. `<width> ' <sub>` — same shape as
  // `SignExtendExpr` but distinct AST node (M3/M5 treat them
  // differently); same recursion need.
  std::vector<const ::nsl::ast::ASTNode *> children;
  children.reserve(2);
  if (node.width() != nullptr) {
    children.push_back(node.width());
  }
  if (node.sub() != nullptr) {
    children.push_back(node.sub());
  }
  return interleaveChildren(node.loc(), children);
}

DocPtr LayoutPlanner::formatNode(const ::nsl::ast::FieldAccessExpr &node) {
  // Recursion-only override. `<obj>.<field>` — `field` is a bare
  // Identifier (not an AST child), so we descend only into `obj`
  // so any nested operator / slice / etc. inside it gets the
  // canonical layout.
  std::vector<const ::nsl::ast::ASTNode *> children;
  if (node.obj() != nullptr) {
    children.push_back(node.obj());
  }
  return interleaveChildren(node.loc(), children);
}

DocPtr LayoutPlanner::formatNode(const ::nsl::ast::IncDecExpr &node) {
  // Recursion-only override. Increment / decrement at expression
  // position — `++x` / `x++` / `--x` / `x--`. Parallels
  // `IncDecStmt` but no trailing `;` (the verbatim parent gap
  // supplies it when relevant).
  std::vector<const ::nsl::ast::ASTNode *> children;
  if (node.target() != nullptr) {
    children.push_back(node.target());
  }
  return interleaveChildren(node.loc(), children);
}

DocPtr LayoutPlanner::formatNode(const ::nsl::ast::TopLevelParamDecl &node) {
  // Recursion-only override. `param_int <name> = <init>;` /
  // `param_str <name> = <init>;` (the latter typically a
  // string-literal). Recurse into `init` so any nested
  // compile-time expression inside fires its canonical layout.
  std::vector<const ::nsl::ast::ASTNode *> children;
  if (node.init() != nullptr) {
    children.push_back(node.init());
  }
  return interleaveChildren(node.loc(), children);
}

DocPtr LayoutPlanner::formatNode(const ::nsl::ast::PortDecl &node) {
  // Recursion-only override. Data terminal:
  //   `<direction> <name>[<width>]` (width nullable).
  // Control terminal: `func_in <name>(<dummy_args>) [: <return>]`
  //   (no width Expr; `dummyArgs` are Identifiers and
  //   `returnTerminal` is an Identifier — neither is an AST
  //   child).
  // So the only Expr-bearing slot to recurse into is `width`.
  std::vector<const ::nsl::ast::ASTNode *> children;
  if (node.width() != nullptr) {
    children.push_back(node.width());
  }
  return interleaveChildren(node.loc(), children);
}

DocPtr LayoutPlanner::formatNode(const ::nsl::ast::VariableDecl &node) {
  // Recursion-only override. `variable <name>[<width>];` —
  // structurally a `WireDecl` look-alike per data-model §1.4
  // (Sema treats them differently). Recurse into `width` so any
  // nested SliceExpr / ConcatExpr / BinaryExpr inside fires the
  // canonical R4 / R5 treatment.
  std::vector<const ::nsl::ast::ASTNode *> children;
  if (node.width() != nullptr) {
    children.push_back(node.width());
  }
  return interleaveChildren(node.loc(), children);
}

DocPtr LayoutPlanner::formatNode(const ::nsl::ast::MemDecl &node) {
  // Recursion-only override. `mem <name>[<depth>][<width>] = (
  //   <init0>, <init1>, ... );` — `depth` and `width` are single
  // Exprs; `init` is a vector of Exprs (one per element) and may
  // be empty. Source order is depth → width → init[0..N].
  std::vector<const ::nsl::ast::ASTNode *> children;
  children.reserve(2 + node.init().size());
  if (node.depth() != nullptr) {
    children.push_back(node.depth());
  }
  if (node.width() != nullptr) {
    children.push_back(node.width());
  }
  for (const auto &n : node.init()) {
    children.push_back(n.get());
  }
  return interleaveChildren(node.loc(), children);
}

DocPtr LayoutPlanner::formatNode(const ::nsl::ast::DelayTaskStmt &node) {
  // Recursion-only override. `_delay(<count>);` system task at
  // statement position. Recurse into the single `count` Expr.
  std::vector<const ::nsl::ast::ASTNode *> children;
  if (node.count() != nullptr) {
    children.push_back(node.count());
  }
  return interleaveChildren(node.loc(), children);
}

DocPtr LayoutPlanner::formatNode(const ::nsl::ast::InitBlockStmt &node) {
  // Recursion-only override. `_init { items... }` — sim-time
  // reset block (`lang.ebnf §10`; lowered M6 under
  // `sv.ifdef "SIMULATION"`). Recurse into each child Stmt so
  // nested transfers / system tasks fire their canonical layout.
  std::vector<const ::nsl::ast::ASTNode *> children;
  children.reserve(node.items().size());
  for (const auto &n : node.items()) {
    children.push_back(n.get());
  }
  return interleaveChildren(node.loc(), children);
}

DocPtr LayoutPlanner::formatNode(const ::nsl::ast::LabeledStmt &node) {
  // Recursion-only override. `<label>: <body>` — `label` is a
  // bare Identifier (not an AST child) so we descend only into
  // `body`. Per N10 the user-supplied label name is permitted
  // even when it shadows the reserved word `label` (lex-time
  // disambiguation).
  std::vector<const ::nsl::ast::ASTNode *> children;
  if (node.body() != nullptr) {
    children.push_back(node.body());
  }
  return interleaveChildren(node.loc(), children);
}

DocPtr LayoutPlanner::formatNode(const ::nsl::ast::StructCastExpr &node) {
  // Recursion-only override. `<typeName>'(<sub>)` or
  // `<typeName>.<member.path>'(<sub>)` per data-model §1.6 —
  // `typeName` and `memberPath` are bare Identifiers (not AST
  // children); the only Expr-bearing slot is `sub`.
  std::vector<const ::nsl::ast::ASTNode *> children;
  if (node.sub() != nullptr) {
    children.push_back(node.sub());
  }
  return interleaveChildren(node.loc(), children);
}

DocPtr LayoutPlanner::formatNode(const ::nsl::ast::StructInstDecl &node) {
  // Recursion-only override. `<typeName> <instanceName>[<arraySize>]
  //   = (<init0>, <init1>, ...);` (`reg` or `wire` storage per
  // `storageKind`; the keyword precedes `typeName` in source).
  // Source order: arraySize → init[0..N], which matches accessor
  // order — no sort needed.
  std::vector<const ::nsl::ast::ASTNode *> children;
  children.reserve(1 + node.init().size());
  if (node.arraySize() != nullptr) {
    children.push_back(node.arraySize());
  }
  for (const auto &n : node.init()) {
    children.push_back(n.get());
  }
  return interleaveChildren(node.loc(), children);
}

DocPtr LayoutPlanner::formatNode(const ::nsl::ast::SubmoduleDecl &node) {
  // Recursion-only override. `<templateName> #(<paramAssigns>)
  //   <instances>;` — `templateName` is an Identifier; the only
  // Expr-bearing slots are `Instance::arraySize` and
  // `ParamAssign::value`, each tagged with its own SourceRange
  // inside the submodule's outer span. Surface-order placement
  // of `paramAssigns` vs `instances` is grammar-dependent (the
  // declared order is paramAssigns then instances per the EBNF;
  // the audited-corpus shape matches), so we gather all Expr
  // children then sort by source offset before interleaving —
  // same precedent as `ModuleBlock`.
  std::vector<const ::nsl::ast::ASTNode *> children;
  children.reserve(node.instances().size() + node.paramAssigns().size());
  for (const auto &inst : node.instances()) {
    if (inst.arraySize != nullptr) {
      children.push_back(inst.arraySize.get());
    }
  }
  for (const auto &pa : node.paramAssigns()) {
    if (pa.value != nullptr) {
      children.push_back(pa.value.get());
    }
  }
  std::sort(children.begin(), children.end(),
            [](const ::nsl::ast::ASTNode *a, const ::nsl::ast::ASTNode *b) {
              return a->loc().begin().offset() < b->loc().begin().offset();
            });
  return interleaveChildren(node.loc(), children);
}

DocPtr LayoutPlanner::formatCondCaseBlock(
    const std::vector<::nsl::ast::CondCase> &cases,
    const ::nsl::ast::Stmt *elseCase, llvm::StringRef keyword) {
  // R1 §1: in `<keyword> { cond : body; ... }` the `:` separators
  // align in the same source column. The column is anchored on the
  // longest condition expression — one space follows the longest
  // condition; shorter conditions are padded.
  //
  // Width measurement uses the source-byte span of `cond->loc()`
  // as a proxy for the canonical-rendered width. This is exact
  // when the source already carries canonical spacing (the case
  // for the T033 fixture and the audited NSL corpus); for inputs
  // with non-canonical spacing in conditions the alignment can be
  // off by the spacing-delta. A future revision should render each
  // cond Doc to a flat string and measure that — `flatWidth` in
  // LayoutRenderer.cpp is the natural extension point.
  std::size_t max_cond = 0;
  for (const auto &c : cases) {
    if (c.cond) {
      auto cr = c.cond->loc();
      std::size_t w = cr.end().offset() - cr.begin().offset();
      if (w > max_cond) {
        max_cond = w;
      }
    }
  }

  std::vector<DocPtr> body_parts;
  body_parts.reserve(cases.size() * 6 + (elseCase ? 4 : 0));
  for (const auto &c : cases) {
    body_parts.push_back(Doc::hardline());
    body_parts.push_back(c.cond ? visitNode(*c.cond)
                                  : Doc::text(llvm::StringRef{}));
    const std::size_t cond_w =
        c.cond ? (c.cond->loc().end().offset() - c.cond->loc().begin().offset())
               : std::size_t{0};
    const std::size_t pad = cfg_.align_case_arrows
                                ? ((max_cond + 1) - cond_w)
                                : std::size_t{1};
    const std::string padding(pad, ' ');
    body_parts.push_back(Doc::text(llvm::StringRef(padding)));
    body_parts.push_back(Doc::text(llvm::StringRef(": ")));
    body_parts.push_back(c.body ? visitNode(*c.body)
                                  : Doc::text(llvm::StringRef{}));
  }
  if (elseCase != nullptr) {
    body_parts.push_back(Doc::hardline());
    body_parts.push_back(Doc::text(llvm::StringRef("else: ")));
    body_parts.push_back(visitNode(*elseCase));
  }

  // K&R (default) vs Allman: same toggle as `formatNode(StructDecl)`.
  // K&R puts the `{` on the same line as the keyword; Allman puts
  // it on its own line at the same indent as the keyword.
  const bool allman = cfg_.brace_style == Configuration::BraceStyle::Allman;
  std::vector<DocPtr> top_parts;
  top_parts.reserve(6);
  top_parts.push_back(Doc::text(keyword));
  if (allman) {
    top_parts.push_back(Doc::hardline());
    top_parts.push_back(Doc::text(llvm::StringRef("{")));
  } else {
    top_parts.push_back(Doc::text(llvm::StringRef(" {")));
  }
  top_parts.push_back(
      Doc::nest(indentStep(), Doc::concat(std::move(body_parts))));
  top_parts.push_back(Doc::hardline());
  top_parts.push_back(Doc::text(llvm::StringRef("}")));
  return Doc::concat(std::move(top_parts));
}

DocPtr LayoutPlanner::formatNode(const ::nsl::ast::TransferStmt &node) {
  // Canonical transfer: `<lhs> <op> <rhs>;` with one space on
  // each side of the assignment operator. Op spelling per
  // `TransferStmt::Op`:
  //   * `WireEq`     → `=`   (combinational `wire_decl = expr;`
  //                    or top-level transfer)
  //   * `RegColonEq` → `:=`  (sequential `reg_name := expr;` per S3)
  // Recurse into both lhs and rhs so any nested binary/unary/
  // slice/concat expression inside fires the R5 / R4 layout —
  // without this override, TransferStmt was a verbatim leaf and
  // `q := a+b;` came out as-is rather than `q := a + b;`.
  llvm::StringRef op =
      node.op() == ::nsl::ast::TransferStmt::Op::RegColonEq
          ? llvm::StringRef(":=")
          : llvm::StringRef("=");
  DocPtr lhs_doc = node.lhs() != nullptr ? visitNode(*node.lhs())
                                          : Doc::text(llvm::StringRef{});
  DocPtr rhs_doc = node.rhs() != nullptr ? visitNode(*node.rhs())
                                          : Doc::text(llvm::StringRef{});
  std::vector<DocPtr> parts;
  parts.reserve(6);
  parts.push_back(std::move(lhs_doc));
  parts.push_back(Doc::text(llvm::StringRef(" ")));
  parts.push_back(Doc::text(op));
  parts.push_back(Doc::text(llvm::StringRef(" ")));
  parts.push_back(std::move(rhs_doc));
  parts.push_back(Doc::text(llvm::StringRef(";")));
  return Doc::concat(std::move(parts));
}

DocPtr LayoutPlanner::formatNode(const ::nsl::ast::ConcatExpr &node) {
  // R4 §4: concatenation is `{<part>, <part>, ...}` — one space
  // after each `,`, no spaces inside `{` / `}` UNLESS the active
  // configuration has `spaces_inside_braces=true`, in which case
  // an extra space pads each side of the brace pair.
  const bool inside = cfg_.spaces_inside_braces;
  llvm::StringRef open_brace = inside ? llvm::StringRef("{ ")
                                       : llvm::StringRef("{");
  llvm::StringRef close_brace = inside ? llvm::StringRef(" }")
                                        : llvm::StringRef("}");
  std::vector<DocPtr> parts;
  // worst case: 1 (open) + 2*N - 1 (parts + separators) + 1 (close)
  parts.reserve(2 + 2 * node.parts().size());
  parts.push_back(Doc::text(open_brace));
  bool first = true;
  for (const auto &p : node.parts()) {
    if (!first) {
      parts.push_back(Doc::text(llvm::StringRef(", ")));
    }
    parts.push_back(p ? visitNode(*p) : Doc::text(llvm::StringRef{}));
    first = false;
  }
  parts.push_back(Doc::text(close_brace));
  return Doc::concat(std::move(parts));
}

// ---- Op-spelling tables -------------------------------------------------

llvm::StringRef
LayoutPlanner::binaryOpSpelling(::nsl::ast::BinaryExpr::Op op) {
  using Op = ::nsl::ast::BinaryExpr::Op;
  switch (op) {
  case Op::Add:
    return "+";
  case Op::Sub:
    return "-";
  case Op::Mul:
    return "*";
  case Op::Div:
    return "/";
  case Op::Mod:
    return "%";
  case Op::BitAnd:
    return "&";
  case Op::BitOr:
    return "|";
  case Op::BitXor:
    return "^";
  case Op::ShiftLeft:
    return "<<";
  case Op::ShiftRight:
    return ">>";
  case Op::Equal:
    return "==";
  case Op::NotEqual:
    return "!=";
  case Op::Less:
    return "<";
  case Op::LessEqual:
    return "<=";
  case Op::Greater:
    return ">";
  case Op::GreaterEqual:
    return ">=";
  case Op::LogicalAnd:
    return "&&";
  case Op::LogicalOr:
    return "||";
  }
  return llvm::StringRef{};
}

llvm::StringRef
LayoutPlanner::unaryOpSpelling(::nsl::ast::UnaryExpr::Op op) {
  using Op = ::nsl::ast::UnaryExpr::Op;
  switch (op) {
  case Op::Neg:
    return "-";
  case Op::Plus:
    return "+";
  case Op::BitNot:
    return "~";
  case Op::LogicalNot:
    return "!";
  case Op::ReduceAnd:
    return "&";
  case Op::ReduceOr:
    return "|";
  case Op::ReduceXor:
    return "^";
  }
  return llvm::StringRef{};
}

} // namespace nsl::fmt
