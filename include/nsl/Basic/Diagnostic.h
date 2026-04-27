// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// include/nsl/Basic/Diagnostic.h
//
// `DiagnosticEngine` is the sole emitter of diagnostics for any
// layer in `libNSLFrontend.a` (FR-024). Direct writes to `stderr`
// from inside the frontend libraries are forbidden — every error,
// warning, and note routes through this engine so M1 honors
// Constitution Principle IV ("Diagnostics First") end-to-end.
//
// Output formats (data-model entity 6):
//   - Text: `<path>:<line>:<col>: <severity>: <message>` plus
//     optional source-line + caret + include-from notes, sorted by
//     (loc, severity) at render time for determinism (research §4).
//   - JSON: NDJSON, one object per line, five mandatory fields per
//     research §9. Smoke-only at M1; schema lock defers to T3.

#ifndef NSL_BASIC_DIAGNOSTIC_H
#define NSL_BASIC_DIAGNOSTIC_H

#include "nsl/Basic/SourceLocation.h"

#include "llvm/ADT/ArrayRef.h"
#include "llvm/Support/raw_ostream.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace nsl {

class SourceManager;

/// Severity ordering: `Note < Warning < Error`. The numeric values
/// are load-bearing for `(loc, severity)` sort-on-render.
enum class Severity : uint8_t {
  Note = 0,
  Warning = 1,
  Error = 2,
};

struct FixItHint {
  SourceRange range;
  std::string replacement;
};

struct Diagnostic {
  Severity severity;
  SourceLocation loc;
  std::string message;
  std::vector<FixItHint> fixits;
  /// Trailing notes attached to this diagnostic (e.g., the
  /// include-from chain for FR-026). Stored alongside so they
  /// render together with the parent regardless of `(loc, severity)`
  /// sort order.
  std::vector<Diagnostic> notes;
  /// Marker flag: when true, the renderer emits this note as
  /// `note: included from <path>:<line>:<col>` per the
  /// `diagnostic-output.contract.md` include-stack form, rather
  /// than the canonical `<path>:<line>:<col>: note: <message>`.
  bool is_include_from_note = false;
};

class DiagnosticEngine {
public:
  enum class Format : uint8_t { Text, JSON };

  /// Builder returned by `report()`. Provides a fluent API for
  /// attaching fixits and include-stack notes to the just-emitted
  /// diagnostic.
  class Builder {
  public:
    Builder(DiagnosticEngine *engine, size_t index)
        : engine_(engine), index_(index) {}

    /// Attach a fixit hint to the current diagnostic.
    Builder &addFixIt(SourceRange range, std::string replacement);

    /// Read the SourceManager's active include stack and append one
    /// `note: included from <path>:<line>:<col>` per ancestor file
    /// (FR-026; innermost first). Read at emit time so the notes
    /// reflect the stack as it existed when this diagnostic was raised.
    Builder &addIncludedFromNotes();

  private:
    DiagnosticEngine *engine_;
    size_t index_;
  };

  explicit DiagnosticEngine(SourceManager &sm);
  ~DiagnosticEngine();

  DiagnosticEngine(const DiagnosticEngine &) = delete;
  DiagnosticEngine &operator=(const DiagnosticEngine &) = delete;

  /// Emit a diagnostic. Returns a `Builder` for further attachment.
  Builder report(Severity sev, SourceLocation loc, std::string msg);

  /// Render every buffered diagnostic to `os` in `fmt` format,
  /// sorted by `(loc, severity)` for determinism (FR-039).
  void renderAll(llvm::raw_ostream &os, Format fmt) const;

  [[nodiscard]] size_t numErrors() const noexcept;
  [[nodiscard]] size_t numWarnings() const noexcept;
  [[nodiscard]] bool hasError() const noexcept { return numErrors() > 0; }
  void clear() noexcept;

  /// Read-only access to the buffered diagnostics for tests / LSP.
  [[nodiscard]] llvm::ArrayRef<Diagnostic> diagnostics() const noexcept;

  /// Engine-internal: attach a fixit to the diagnostic at `index`.
  /// Public so the `Builder` returned by `report()` can call it.
  void appendFixItAt(size_t index, FixItHint hint);

  /// Engine-internal: attach a note to the diagnostic at `index`.
  void appendNoteAt(size_t index, Diagnostic note);

  /// Engine-internal: access the bound `SourceManager` for the
  /// `Builder` to read the include stack.
  [[nodiscard]] SourceManager &sourceManager() const noexcept;

private:
  class Impl;
  std::unique_ptr<Impl> impl_;
};

} // namespace nsl

#endif // NSL_BASIC_DIAGNOSTIC_H
