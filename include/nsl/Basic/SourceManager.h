// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// include/nsl/Basic/SourceManager.h
//
// `SourceManager` owns one `Buffer` per loaded file, allocates
// `FileID`s, resolves `(file, line, col)` ↔ byte-offset queries, and
// honors `#line` adjustments such that a given `SourceLocation` can
// resolve to either the *physical* file:line:col or the *logical*
// (post-`#line`) virtual file:line:col (data-model entity 5;
// research §3).
//
// The implementation is M1's own — not `llvm::SourceMgr`. The latter
// lacks the `#line` virtual-file machinery NSL requires (Principle
// IV; research §3 alternatives); the former is a fraction of
// clang's `clang::SourceManager`, sized for NSL.

#ifndef NSL_BASIC_SOURCEMANAGER_H
#define NSL_BASIC_SOURCEMANAGER_H

#include "nsl/Basic/SourceLocation.h"

#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "llvm/ADT/StringRef.h"
#include "llvm/Support/ErrorOr.h"

namespace nsl {

namespace detail {
class Buffer;
}

class SourceManager {
public:
  SourceManager();
  ~SourceManager();

  // Movable but not copyable.
  SourceManager(const SourceManager &) = delete;
  SourceManager &operator=(const SourceManager &) = delete;
  SourceManager(SourceManager &&) noexcept;
  SourceManager &operator=(SourceManager &&) noexcept;

  // ------------------ File loading ------------------

  /// Read `path` from disk and register a `Buffer` for it. Returns
  /// the `FileID` of the loaded buffer, or an error if the file
  /// cannot be opened.
  ///
  /// **Idempotent**: loading the same canonical absolute path twice
  /// returns the same `FileID` — required for correct cycle detection
  /// in the include stack.
  llvm::ErrorOr<FileID> loadFile(llvm::StringRef path);

  /// Register an in-memory buffer with `path` as its label. Used by
  /// tests and by callers that have already read the bytes (e.g., the
  /// future LSP path).
  FileID addBufferInMemory(std::string path, std::vector<char> bytes);

  /// Bytes of the buffer for `f` (NUL-terminator NOT included in the
  /// returned `StringRef`).
  llvm::StringRef getBuffer(FileID f) const;

  /// The path label registered for `f`.
  llvm::StringRef getPath(FileID f) const;

  // ------------------ Physical location queries ------------------

  /// 1-based `(line, col)` for `loc` against its physical file.
  /// O(log lines) after the lazy line-offset table is built.
  std::pair<uint32_t, uint32_t> getLineCol(SourceLocation loc) const;

  /// The raw bytes of the source line containing `loc` (excluding
  /// the trailing newline).
  llvm::StringRef getLine(SourceLocation loc) const;

  // ------------------ Virtual location queries (post-#line) ------------------

  struct VirtualLoc {
    llvm::StringRef path;
    uint32_t line;
    uint32_t col;
  };

  /// Resolve `loc` to its post-`#line` virtual coordinates if any
  /// matching `LineDirective` exists at or before `loc.offset()`;
  /// otherwise returns the physical coordinates.
  VirtualLoc resolveVirtual(SourceLocation loc) const;

  // ------------------ #line directive registration ------------------

  /// Register a `#line` directive seen by the preprocessor. `at` is
  /// the byte offset of the FIRST byte AFTER the directive (where the
  /// override takes effect). `virtual_line` is the new line number to
  /// assign that byte; `virtual_path` is the new path label (empty =
  /// reuse current).
  ///
  /// **Precondition**: `at.offset()` must be strictly greater than
  /// the most recent override registered for the same file
  /// (data-model entity 5 invariant; aborts otherwise).
  void addLineDirective(SourceLocation at, uint32_t virtual_line,
                        llvm::StringRef virtual_path);

  // ------------------ Include-stack tracking (for diagnostics) ------------------

  /// Push a new include frame. `include_directive_loc` is the
  /// `SourceLocation` of the `#include` directive that resolved to
  /// `included`. The DiagnosticEngine reads the active stack at
  /// emit time to populate include-from notes (FR-026).
  void pushIncludeFrame(SourceLocation include_directive_loc, FileID included);

  /// Pop the most recently pushed include frame.
  void popIncludeFrame();

  /// Return the chain of `#include` directive locations that led to
  /// `f`, innermost first. Empty when `f` is the original input file
  /// or when no include frame for `f` is currently active.
  std::vector<SourceLocation> getIncludeStackFor(FileID f) const;

private:
  // Pimpl-style impl pointer to keep the public header stable while
  // the `Buffer`-vector layout evolves over later milestones.
  class Impl;
  std::unique_ptr<Impl> impl_;
};

} // namespace nsl

#endif // NSL_BASIC_SOURCEMANAGER_H
