//===- CST.h - Concrete syntax tree for nsl-fmt ----------------*- C++ -*-=//
//
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Concrete syntax tree (CST) types for `libNslFmt.a`. Per Principle II
// these types are INTERNAL to lib/Fmt/ — `Fmt.h` (the sole public
// umbrella header) does NOT export them. Frozen by
// `specs/010-t2-formatter-v0/contracts/cst-shape.contract.md` §1, §3,
// §5; instances are constructed by `DirectiveSplitter` (slices) and
// `CSTBuilder` (interior `CSTNode` instances + `Trivia`); they are
// consumed by `LayoutPlanner`.
//
// Data only — no method bodies beyond constructors. Lifetime of the
// `StringRef` text spans is tied to the source `MemoryBuffer`'s
// lifetime per cst-shape contract §3.
//
//===----------------------------------------------------------------------===//

#ifndef NSL_FMT_LIB_CST_H
#define NSL_FMT_LIB_CST_H

#include "nsl/Basic/SourceLocation.h"
#include "nsl/Lex/Token.h"

#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/StringRef.h"

#include <cstdint>
#include <vector>

namespace nsl::fmt {

/// Stable opaque identifier for a CST node, monotonic within one parse.
/// `0` is reserved for "no node".
using NodeID = std::uint64_t;

/// Trivia — preserved whitespace / comment between tokens. Per
/// cst-shape contract §4 attachment rules.
struct Trivia {
  enum class Kind { Whitespace, LineComment, BlockComment, Newline };

  Kind             kind;
  llvm::StringRef  text;   // Byte-for-byte from source buffer.
  ::nsl::SourceLocation loc;
};

/// Preprocessor directive token — opaque, byte-preserved by the
/// formatter (FR-012a). Per cst-shape contract §5.
struct DirectiveTok {
  enum class Opcode {
    Include,    // `#include "foo.nsl"` or `#include <foo.nsl>`
    Define,     // `#define FOO ...`
    Undef,      // `#undef FOO`
    Ifdef,      // `#ifdef FOO`
    Ifndef,     // `#ifndef FOO`
    If,         // `#if EXPR`
    Else,       // `#else`
    Endif,      // `#endif`
    Line,       // `#line N "file"` (the only directive that survives the seam — Principle IV)
  };

  Opcode               opcode;
  ::nsl::SourceRange   range;
  llvm::StringRef      rawText; // Entire directive line(s), byte-for-byte.
};

/// Top-level slice produced by the directive splitter — a 1:1 partition
/// of the source-buffer bytes (no gaps, no overlaps; cst-shape §1).
struct Slice {
  enum class Kind {
    Directive,    // A complete directive line; `directive` carries its details.
    NSLFragment,  // A range of NSL source between directives.
  };

  Kind                 kind;
  ::nsl::SourceRange   range;
  llvm::StringRef      rawText;
  DirectiveTok         directive; // Valid iff kind == Directive.
};

/// One source token (lexer emits these). Phase 2b populates the
/// CSTBuilder which records these as leaves of interior nodes.
struct NSLToken {
  nsl::TokenKind       kind;
  llvm::StringRef      lexeme; // Byte-for-byte from source.
  ::nsl::SourceRange   range;
};

/// Interior CST node — mirrors one AST node 1:1, plus preserved
/// trivia. Children appear in source order; every byte of `range`
/// is covered either by a child, by a trivia in `leading`/`trailing`,
/// or by the node's own token (cst-shape §3 no-byte-loss invariant).
struct CSTNode {
  NodeID                              id   = 0;
  ::nsl::SourceRange                  range;
  llvm::SmallVector<CSTNode *, 4>     children;
  std::vector<Trivia>                 leadingTrivia;
  std::vector<Trivia>                 trailingTrivia;
};

/// Root of one parsed file — a sequence of slices in source order.
struct SourceFile {
  std::vector<Slice> slices;
};

} // namespace nsl::fmt

#endif // NSL_FMT_LIB_CST_H
