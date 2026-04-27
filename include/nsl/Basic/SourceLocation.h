// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// include/nsl/Basic/SourceLocation.h
//
// Foundational source-attribution types for `nsl-basic` (data-model
// entities 1–3 in `specs/002-m1-lex-preprocess/data-model.md`).
//
// `SourceLocation` is an opaque 32-bit handle: 24 bits of byte offset
// within a `Buffer` plus 8 bits of `FileID` index. The 24/8 split
// gives 16 MiB per file × 256 files, which is comfortably above any
// plausible NSL input at M1 (research §3).
//
// Every Token, every diagnostic, and (later) every AST node carries
// a `SourceLocation` or `SourceRange`. Constitution Principle IV
// makes loss of source attribution a hard invariant violation, so the
// types exposed here are the foundation on which every later layer
// builds.

#ifndef NSL_BASIC_SOURCELOCATION_H
#define NSL_BASIC_SOURCELOCATION_H

#include <cstdint>

namespace nsl {

class SourceManager;

/// Identifier of a buffer registered with the `SourceManager`.
///
/// Encoded into the top 8 bits of `SourceLocation::bits_`. The zero
/// value is the invalid sentinel; valid IDs are allocated by
/// `SourceManager` in insertion order in `[1, 255]`.
class FileID {
public:
  /// Default-construct the invalid sentinel.
  FileID() noexcept : id_(0) {}

  /// Construct from a raw `uint8_t`.
  ///
  /// In production, `FileID`s are minted by `SourceManager`. This
  /// constructor is public so unit tests can construct `FileID`s in
  /// isolation; user code should obtain `FileID`s from
  /// `SourceManager::loadFile` or `SourceManager::addBufferInMemory`.
  explicit FileID(uint8_t id) noexcept : id_(id) {}

  [[nodiscard]] uint8_t raw() const noexcept { return id_; }
  [[nodiscard]] bool isValid() const noexcept { return id_ != 0; }

  bool operator==(FileID other) const noexcept { return id_ == other.id_; }
  bool operator!=(FileID other) const noexcept { return id_ != other.id_; }
  bool operator<(FileID other) const noexcept { return id_ < other.id_; }

private:
  uint8_t id_;
};

/// The smallest unit of source attribution: a `(FileID, offset)` pair
/// packed into a single 32-bit word.
class SourceLocation {
public:
  /// Maximum representable byte offset (exclusive). Equivalent to
  /// 16 MiB; offsets in `[0, kMaxOffset)` are valid.
  static constexpr uint32_t kMaxOffset = 1U << 24;

  /// Default-construct the invalid sentinel.
  SourceLocation() noexcept : bits_(0) {}

  /// Pack `(fid, off)` into a `SourceLocation`. Aborts via `assert`
  /// if `off >= kMaxOffset` (24-bit field overflow).
  static SourceLocation make(FileID fid, uint32_t off);

  [[nodiscard]] FileID file() const noexcept {
    return FileID(static_cast<uint8_t>((bits_ >> 24) & 0xFFU));
  }

  [[nodiscard]] uint32_t offset() const noexcept { return bits_ & 0x00FFFFFFU; }

  /// True iff this is not the default-constructed sentinel.
  ///
  /// A `SourceLocation` is invalid when its packed bits are zero —
  /// i.e., `FileID == 0` and `offset == 0`. In production no
  /// `FileID` of zero is ever minted, so a zero value uniquely
  /// identifies "no location".
  [[nodiscard]] bool isValid() const noexcept { return bits_ != 0; }

  bool operator==(SourceLocation other) const noexcept {
    return bits_ == other.bits_;
  }
  bool operator!=(SourceLocation other) const noexcept {
    return bits_ != other.bits_;
  }

  /// Total order: primary by `FileID`, secondary by offset.
  bool operator<(SourceLocation other) const noexcept {
    // The packed layout `(file << 24) | offset` already sorts in
    // (file, offset) lexicographic order under unsigned compare, so
    // a single compare on the raw bits suffices.
    return bits_ < other.bits_;
  }

  /// Internal accessor used by `SourceManager` for testing /
  /// serialization. Not intended for general use.
  [[nodiscard]] uint32_t rawBits() const noexcept { return bits_; }

private:
  uint32_t bits_;
};

/// A half-open `[begin, end)` span of source whose endpoints are
/// constrained to live in the same `FileID`.
class SourceRange {
public:
  /// Default-construct the invalid range.
  SourceRange() noexcept = default;

  /// Construct a range. Aborts via `assert` if `b` and `e` come from
  /// different files or if `b > e`.
  SourceRange(SourceLocation b, SourceLocation e);

  [[nodiscard]] SourceLocation begin() const noexcept { return begin_; }
  [[nodiscard]] SourceLocation end() const noexcept { return end_; }

  /// Length in bytes (0 for an empty range).
  [[nodiscard]] uint32_t length() const noexcept {
    return end_.offset() - begin_.offset();
  }

  /// True iff `loc` is `>= begin()` and `< end()` and lives in the
  /// same `FileID`.
  [[nodiscard]] bool contains(SourceLocation loc) const noexcept {
    if (!loc.isValid() || !isValid()) {
      return false;
    }
    if (loc.file() != begin_.file()) {
      return false;
    }
    return !(loc < begin_) && (loc < end_);
  }

  /// True iff both endpoints are valid (and, by construction, live
  /// in the same file).
  [[nodiscard]] bool isValid() const noexcept {
    return begin_.isValid() && end_.isValid();
  }

private:
  SourceLocation begin_;
  SourceLocation end_;
};

} // namespace nsl

#endif // NSL_BASIC_SOURCELOCATION_H
