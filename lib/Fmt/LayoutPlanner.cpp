// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// lib/Fmt/LayoutPlanner.cpp — Phase 3-skeleton implementation.
// Verbatim-fallback only; Phase 3-rules replaces per-node-kind
// handlers incrementally.

#include "LayoutPlanner.h"

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
      parts.push_back(verbatimFromOffsets(cursor, child_begin));
    }
    parts.push_back(visitNode(*child));
    cursor = child_end;
  }
  if (cursor < parent_end) {
    parts.push_back(verbatimFromOffsets(cursor, parent_end));
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
#define NSL_NODE_KIND(EnumName, BaseClass)                                     \
  void LayoutPlanner::visit(const ::nsl::ast::EnumName &node) {                \
    result_ = formatNode(node);                                                \
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
  // Four declaration-order vectors (per ModuleBlock.h) merge into a
  // single source-position-sorted list so `interleaveChildren` can
  // walk them as a flat sequence. The four vectors are sorted by
  // KIND, not source order, so we sort here.
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
  return interleaveChildren(node.loc(), children);
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
