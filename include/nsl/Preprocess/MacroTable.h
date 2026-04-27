// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// lib/Preprocess/MacroTable.h — PRIVATE header for nsl-preprocess.
//
// Implements data-model entity 11 (`MacroTable`) and its companion
// entity 10 (`MacroDef`). The storage type is `llvm::MapVector` so
// iteration order is INSERTION ORDER (FR-039), not hash-derived.
// This is the canonical Principle V determinism guard for the
// preprocessor (research §4) and is the binding choice from
// `specs/002-m1-lex-preprocess/contracts/preprocessor-seam.contract.md`.
//
// This header is private to lib/Preprocess/ — it lives alongside the
// implementation rather than under include/nsl/Preprocess/ so the
// public API surface stays minimal (FR-002 / Principle II — only
// `Preprocessor.h` is exported).

#ifndef NSL_LIB_PREPROCESS_MACROTABLE_H
#define NSL_LIB_PREPROCESS_MACROTABLE_H

#include "nsl/Basic/SourceLocation.h"

#include "llvm/ADT/MapVector.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/StringRef.h"

#include <map>
#include <string>

namespace nsl::preprocess {

/// Storage record for a `#define`'d macro (data-model entity 10).
///
/// Bodies are stored UNEXPANDED — expansion is performed at the use
/// site per **P10**. The owning `MacroTable` retains the body text as
/// a `std::string` so the body outlives any transient buffer it might
/// have been parsed from (an `#include`'d file's buffer might be popped
/// from the include stack while a later expansion still references the
/// macro).
struct MacroDef {
  /// Macro name (NUL-free; the recognizer rejects empty names).
  std::string name;

  /// Replacement body, stored verbatim from the `#define` line up to
  /// (not including) the trailing newline. Whitespace at the start
  /// of the body is normalized to a single space at insertion time.
  std::string body;

  /// `SourceRange` of the `#define` directive that introduced this
  /// macro (used by `redefine` to attach a `note: previous definition
  /// was here`).
  SourceRange defining_loc;
};

/// Insertion-ordered map from macro name to `MacroDef`. The
/// `llvm::MapVector` provides O(1) lookup via an internal `DenseMap`
/// while preserving insertion order on iteration (research §4 / §6
/// of `data-model.md`).
class MacroTable {
public:
  MacroTable() = default;

  // Non-copyable; movable. The internal MapVector holds names as
  // `std::string` keys (StringRef into the values would dangle if
  // strings moved on insert).
  MacroTable(const MacroTable &) = delete;
  MacroTable &operator=(const MacroTable &) = delete;
  MacroTable(MacroTable &&) noexcept = default;
  MacroTable &operator=(MacroTable &&) noexcept = default;

  /// Insert a macro definition. Returns `true` if newly inserted,
  /// `false` if an entry of the same name already exists (caller
  /// decides whether to call `redefine` next).
  bool insert(llvm::StringRef name, llvm::StringRef body,
              SourceRange defining_loc);

  /// Replace an existing definition. The previous `defining_loc` is
  /// returned via `out_previous_loc` so the caller can attach a
  /// `note: previous definition was here` to its diagnostic. If no
  /// entry exists, behaves like `insert`.
  void redefine(llvm::StringRef name, llvm::StringRef body,
                SourceRange defining_loc, SourceRange *out_previous_loc);

  /// Look up a macro by name. Returns `nullptr` if no entry exists.
  const MacroDef *lookup(llvm::StringRef name) const;
  MacroDef *lookup(llvm::StringRef name);

  /// Remove a macro. Returns `true` if the entry existed.
  bool undef(llvm::StringRef name);

  /// True iff `name` is defined.
  bool defined(llvm::StringRef name) const { return lookup(name) != nullptr; }

  /// Predefine a macro from a `-D NAME=value` CLI flag. Inserted
  /// before any source-defined macro (per data-model entity 11
  /// "Predefined macros" invariant).
  void predefine(llvm::StringRef name, llvm::StringRef body);

  /// Number of defined macros.
  std::size_t size() const { return entries_.size(); }
  bool empty() const { return entries_.empty(); }

  /// Iteration is insertion-ordered. The yielded `MacroDef`s are
  /// references into the underlying storage and remain valid for the
  /// lifetime of this `MacroTable` (no rehash invalidates them
  /// because `MapVector` is array-backed).
  ///
  /// The default `llvm::MapVector` template parameters use
  /// `DenseMap<KeyT, unsigned>` as the index map, but
  /// `llvm::DenseMapInfo<std::string>` is not specialized. We override
  /// the index-map template parameter with `std::map<std::string,
  /// unsigned>` — still deterministic, still O(log n) lookup, and
  /// correct for `std::string` keys.
  using container_type =
      llvm::MapVector<std::string, MacroDef, std::map<std::string, unsigned>>;
  container_type::iterator begin() { return entries_.begin(); }
  container_type::iterator end() { return entries_.end(); }
  container_type::const_iterator begin() const { return entries_.begin(); }
  container_type::const_iterator end() const { return entries_.end(); }

private:
  container_type entries_;
};

} // namespace nsl::preprocess

#endif // NSL_LIB_PREPROCESS_MACROTABLE_H
