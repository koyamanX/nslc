// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// lib/Basic/SourceManager.cpp — own `SourceManager` per research §3
// (not `llvm::SourceMgr`). Implements data-model entities 4 (private
// `Buffer`) and 5 (`SourceManager` public API).

#include "nsl/Basic/SourceManager.h"

#include "AssertImpl.h"
#include "nsl/Basic/SourceLocation.h"

#include "llvm/ADT/StringRef.h"
#include "llvm/Support/ErrorOr.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <ios>
#include <iterator>
#include <memory>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

namespace nsl {

// -----------------------------------------------------------------------------
// LineDirective + Buffer (private to this TU; data-model entity 4)
// -----------------------------------------------------------------------------

namespace {

struct LineDirective {
  uint32_t origin_offset;
  uint32_t virtual_line;
  std::string virtual_path; // empty == reuse current path
};

struct Buffer {
  std::string path;        // path label (canonical when loadFile)
  std::vector<char> bytes; // NUL-terminated for safety
  // Lazy: built on first (line, col) query. Each entry is the byte
  // offset of the first character of line (i+1). line_offsets[0] == 0.
  mutable std::vector<uint32_t> line_offsets;
  mutable bool line_offsets_built = false;
  // Sorted by origin_offset; binary-searched at query time.
  std::vector<LineDirective> line_overrides;
};

void buildLineOffsetsIfNeeded(const Buffer &b) {
  if (b.line_offsets_built) {
    return;
  }
  b.line_offsets.clear();
  b.line_offsets.push_back(0);
  // bytes is NUL-terminated; iterate up to the visible length.
  // The NUL sentinel is appended in addBufferInMemory; size minus
  // one is the content size.
  const size_t visible = b.bytes.empty() ? 0 : b.bytes.size() - 1;
  for (size_t i = 0; i < visible; ++i) {
    if (b.bytes[i] == '\n') {
      // Line i+2 starts at offset i+1.
      auto next = static_cast<uint32_t>(i + 1);
      b.line_offsets.push_back(next);
    }
  }
  b.line_offsets_built = true;
}

} // namespace

// -----------------------------------------------------------------------------
// Impl
// -----------------------------------------------------------------------------

class SourceManager::Impl {
public:
  // Index 0 is the invalid sentinel — we waste it so FileID(0) is
  // always invalid (matches data-model invariant). Entries [1..N] are
  // real buffers.
  std::vector<std::unique_ptr<Buffer>> buffers;

  // Active include stack. Entry i records: include_directive_loc =
  // location of the `#include` line in the parent file; included =
  // the file pushed onto the stack.
  struct IncludeFrame {
    SourceLocation include_directive_loc;
    FileID included;
  };
  std::vector<IncludeFrame> include_stack;

  // Permanent record of "this FileID was first pulled in by an
  // `#include` at this location" — used by post-preprocessing
  // consumers (Sema, MLIR, LSP) for FR-026 include-from notes.
  // Indexed by `FileID::raw()`; entries default to invalid
  // SourceLocation. FileIDs are bounded to [0, 255] per data-model.
  std::array<SourceLocation, 256> permanent_include_sites;

  Impl() {
    // Seat the index-0 sentinel.
    buffers.push_back(nullptr);
  }

  [[nodiscard]] Buffer *bufferOrNull(FileID f) const {
    if (!f.isValid() || f.raw() >= buffers.size()) {
      return nullptr;
    }
    return buffers[f.raw()].get();
  }

  [[nodiscard]] Buffer &buffer(FileID f) const {
    Buffer *b = bufferOrNull(f);
    NSL_ABORT(b, "FileID out of range");
    return *b;
  }

  FileID allocate(std::string path, std::vector<char> bytes) {
    // Append the NUL sentinel for safe trailing scans (entity 4 invariant).
    bytes.push_back('\0');

    auto buf = std::make_unique<Buffer>();
    buf->path = std::move(path);
    buf->bytes = std::move(bytes);

    NSL_ABORT(buffers.size() <= 255, "FileID exhausted: 256-file limit");
    auto id = static_cast<uint8_t>(buffers.size());
    buffers.push_back(std::move(buf));
    return FileID(id);
  }
};

// -----------------------------------------------------------------------------
// Public API
// -----------------------------------------------------------------------------

SourceManager::SourceManager() : impl_(std::make_unique<Impl>()) {}
SourceManager::~SourceManager() = default;
SourceManager::SourceManager(SourceManager &&) noexcept = default;
SourceManager &SourceManager::operator=(SourceManager &&) noexcept = default;

llvm::ErrorOr<FileID> SourceManager::loadFile(llvm::StringRef path) {
  std::string spath = path.str();

  // Idempotence: return the existing FileID if the same path is
  // already loaded (entity 5 invariant).
  for (size_t i = 1; i < impl_->buffers.size(); ++i) {
    if (impl_->buffers[i]->path == spath) {
      return FileID(static_cast<uint8_t>(i));
    }
  }

  std::ifstream in(spath, std::ios::in | std::ios::binary);
  if (!in.is_open()) {
    return std::make_error_code(std::errc::no_such_file_or_directory);
  }
  std::vector<char> bytes((std::istreambuf_iterator<char>(in)),
                          std::istreambuf_iterator<char>());

  return impl_->allocate(std::move(spath), std::move(bytes));
}

FileID SourceManager::addBufferInMemory(std::string path,
                                        std::vector<char> bytes) {
  return impl_->allocate(std::move(path), std::move(bytes));
}

llvm::StringRef SourceManager::getBuffer(FileID f) const {
  Buffer &b = impl_->buffer(f);
  // Visible bytes exclude the NUL sentinel.
  size_t const visible = b.bytes.empty() ? 0 : b.bytes.size() - 1;
  return {b.bytes.data(), visible};
}

llvm::StringRef SourceManager::getPath(FileID f) const {
  Buffer const &b = impl_->buffer(f);
  return {b.path};
}

std::pair<uint32_t, uint32_t>
SourceManager::getLineCol(SourceLocation loc) const {
  Buffer &b = impl_->buffer(loc.file());
  buildLineOffsetsIfNeeded(b);
  uint32_t const off = loc.offset();
  // Binary search: largest line_offsets[i] <= off.
  auto it = std::upper_bound(b.line_offsets.begin(), b.line_offsets.end(), off);
  // upper_bound gives the first > off; the line index is one before.
  size_t const line_idx =
      static_cast<size_t>(std::distance(b.line_offsets.begin(), it)) - 1U;
  uint32_t const line = static_cast<uint32_t>(line_idx) + 1U; // 1-based
  uint32_t const line_start = b.line_offsets[line_idx];
  uint32_t const col = off - line_start + 1U; // 1-based
  return {line, col};
}

llvm::StringRef SourceManager::getLine(SourceLocation loc) const {
  Buffer &b = impl_->buffer(loc.file());
  buildLineOffsetsIfNeeded(b);
  uint32_t const off = loc.offset();
  auto it = std::upper_bound(b.line_offsets.begin(), b.line_offsets.end(), off);
  size_t const line_idx =
      static_cast<size_t>(std::distance(b.line_offsets.begin(), it)) - 1U;
  uint32_t const line_start = b.line_offsets[line_idx];

  size_t const visible = b.bytes.empty() ? 0 : b.bytes.size() - 1;
  // End of line: next line offset minus 1 (the '\n'), or the
  // visible end of the buffer for the last line.
  size_t line_end = 0;
  if (line_idx + 1 < b.line_offsets.size()) {
    // Subtract one to exclude the '\n' at the end of this line.
    line_end = static_cast<size_t>(b.line_offsets[line_idx + 1]) - 1U;
  } else {
    line_end = visible;
  }
  // Defensive: if the last line happens to end with '\n' (visible
  // ends just past it), trim. The line_offsets builder appends an
  // entry for "after newline", so for "abc\n" line_offsets = [0,4]
  // and line_idx==0 picks line_end=3 correctly via the if-branch.
  return {b.bytes.data() + line_start, line_end - line_start};
}

SourceManager::VirtualLoc
SourceManager::resolveVirtual(SourceLocation loc) const {
  Buffer &b = impl_->buffer(loc.file());

  // Find the active LineDirective: largest origin_offset <= loc.offset().
  const LineDirective *active = nullptr;
  for (const auto &d : b.line_overrides) {
    if (d.origin_offset <= loc.offset()) {
      if ((active == nullptr) || d.origin_offset > active->origin_offset) {
        active = &d;
      }
    } else {
      // Sorted; remaining entries are all > offset.
      break;
    }
  }

  auto [phys_line, phys_col] = getLineCol(loc);

  if (active == nullptr) {
    return VirtualLoc{llvm::StringRef(b.path), phys_line, phys_col};
  }

  // The virtual line at `active->origin_offset` is `virtual_line`.
  // Subsequent physical lines map by simple offset.
  buildLineOffsetsIfNeeded(b);
  // Compute the physical line number at the directive's origin offset.
  uint32_t const origin_off = active->origin_offset;
  auto it = std::upper_bound(b.line_offsets.begin(), b.line_offsets.end(),
                             origin_off);
  size_t const origin_line_idx =
      static_cast<size_t>(std::distance(b.line_offsets.begin(), it)) - 1U;
  uint32_t const origin_phys_line = static_cast<uint32_t>(origin_line_idx) + 1U;

  uint32_t const virt_line =
      active->virtual_line + (phys_line - origin_phys_line);
  llvm::StringRef const virt_path = active->virtual_path.empty()
                                        ? llvm::StringRef(b.path)
                                        : llvm::StringRef(active->virtual_path);
  return VirtualLoc{virt_path, virt_line, phys_col};
}

void SourceManager::addLineDirective(SourceLocation at, uint32_t virtual_line,
                                     llvm::StringRef virtual_path) {
  Buffer &b = impl_->buffer(at.file());
  if (!b.line_overrides.empty()) {
    NSL_ABORT(at.offset() > b.line_overrides.back().origin_offset,
              "addLineDirective: out-of-order insertion");
  }
  b.line_overrides.push_back(
      LineDirective{at.offset(), virtual_line, virtual_path.str()});
}

void SourceManager::pushIncludeFrame(SourceLocation include_directive_loc,
                                     FileID included) {
  impl_->include_stack.push_back(
      Impl::IncludeFrame{include_directive_loc, included});
  // Permanent record (FR-026 post-preprocessing queries). The
  // first-seen include site wins on the unlikely case that the
  // same FileID is re-pushed; in practice `loadFile` is idempotent
  // and a file is only re-included after an intervening pop.
  if (included.isValid()) {
    auto &slot = impl_->permanent_include_sites[included.raw()];
    if (!slot.isValid()) {
      slot = include_directive_loc;
    }
  }
}

void SourceManager::popIncludeFrame() {
  NSL_ABORT(!impl_->include_stack.empty(),
            "popIncludeFrame on empty include stack");
  impl_->include_stack.pop_back();
}

std::vector<SourceLocation> SourceManager::getIncludeStackFor(FileID f) const {
  // Find the topmost frame whose `included` matches `f`. The
  // ancestry from that point downward (innermost first toward root)
  // is the include trace.
  std::vector<SourceLocation> out;
  // Innermost-first: walk backwards from the last frame.
  bool found = false;
  for (auto it = impl_->include_stack.rbegin();
       it != impl_->include_stack.rend(); ++it) {
    if (!found) {
      if (it->included == f) {
        found = true;
        out.push_back(it->include_directive_loc);
      }
      continue;
    }
    out.push_back(it->include_directive_loc);
  }
  return out;
}

FileID SourceManager::findFileIDByPath(llvm::StringRef path) const {
  // Skip the index-0 sentinel.
  for (size_t i = 1; i < impl_->buffers.size(); ++i) {
    const auto &buf = impl_->buffers[i];
    if (buf && buf->path == path) {
      return FileID(static_cast<uint8_t>(i));
    }
  }
  return {};
}

std::vector<SourceLocation>
SourceManager::getOriginalIncludeStackFor(FileID f) const {
  // Walk the permanent include-site map upward from `f` until we
  // hit a file that was not registered as included (i.e., the root
  // input). Innermost first, matching `getIncludeStackFor`'s order.
  std::vector<SourceLocation> out;
  FileID cursor = f;
  // Bound the walk by the buffer count to avoid pathological loops
  // in the unlikely event the permanent map ever forms a cycle.
  for (size_t i = 0; i < impl_->buffers.size(); ++i) {
    if (!cursor.isValid()) {
      break;
    }
    SourceLocation site = impl_->permanent_include_sites[cursor.raw()];
    if (!site.isValid()) {
      break;
    }
    out.push_back(site);
    FileID parent = site.file();
    if (parent == cursor) {
      break;
    }
    cursor = parent;
  }
  return out;
}

} // namespace nsl
