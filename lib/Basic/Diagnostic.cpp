// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// lib/Basic/Diagnostic.cpp — implementation of the diagnostic engine
// declared in `include/nsl/Basic/Diagnostic.h`. Sort-on-render,
// canonical text format, smoke-only NDJSON.

#include "nsl/Basic/Diagnostic.h"

#include "nsl/Basic/SourceLocation.h"
#include "nsl/Basic/SourceManager.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "llvm/ADT/ArrayRef.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/Support/raw_ostream.h"

namespace nsl {

namespace {

const char *severityName(Severity s) {
  switch (s) {
  case Severity::Note:
    return "note";
  case Severity::Warning:
    return "warning";
  case Severity::Error:
    return "error";
  }
  return "unknown";
}

/// Comparator used for `(loc, severity)` sort. Note < Warning <
/// Error (the enum's numeric order).
bool diagLess(const Diagnostic &a, const Diagnostic &b) {
  if (a.loc < b.loc) {
    return true;
  }
  if (b.loc < a.loc) {
    return false;
  }
  return static_cast<uint8_t>(a.severity) < static_cast<uint8_t>(b.severity);
}

void writeTextHeaderLine(llvm::raw_ostream &os, const SourceManager &sm,
                         const Diagnostic &d) {
  auto v = sm.resolveVirtual(d.loc);
  if (d.is_include_from_note) {
    // Include-from form per `diagnostic-output.contract.md`:
    //   `note: included from <path>:<line>:<col>`
    // No leading `<path>:<line>:<col>:` because the note's location
    // IS the include site we're naming.
    os << "note: included from " << v.path << ':' << v.line << ':' << v.col
       << '\n';
    return;
  }
  os << v.path << ':' << v.line << ':' << v.col << ": "
     << severityName(d.severity) << ": " << d.message << '\n';
}

/// Escape a string for inclusion in a JSON string literal per RFC 8259.
/// Manual implementation rather than reaching for `llvm::json::Value`
/// because the latter's `Object` is `DenseMap`-backed (hash iteration
/// order) and would break Principle V determinism.
void writeJsonString(llvm::raw_ostream &os, llvm::StringRef s) {
  os << '"';
  for (unsigned char c : s) {
    switch (c) {
    case '"':
      os << "\\\"";
      break;
    case '\\':
      os << "\\\\";
      break;
    case '\b':
      os << "\\b";
      break;
    case '\f':
      os << "\\f";
      break;
    case '\n':
      os << "\\n";
      break;
    case '\r':
      os << "\\r";
      break;
    case '\t':
      os << "\\t";
      break;
    default:
      if (c < 0x20) {
        // \u00XX form for control characters.
        os << "\\u00";
        constexpr char hex[] = "0123456789abcdef";
        os << hex[(c >> 4) & 0xF] << hex[c & 0xF];
      } else {
        os << static_cast<char>(c);
      }
      break;
    }
  }
  os << '"';
}

void writeJsonObject(llvm::raw_ostream &os, const SourceManager &sm,
                     const Diagnostic &d, const SourceLocation *included_from) {
  auto v = sm.resolveVirtual(d.loc);
  os << '{';
  os << "\"path\":";
  writeJsonString(os, v.path);
  os << ",\"line\":" << v.line;
  os << ",\"col\":" << v.col;
  os << ",\"severity\":";
  writeJsonString(os, severityName(d.severity));
  os << ",\"message\":";
  writeJsonString(os, d.message);
  if (included_from) {
    auto vinc = sm.resolveVirtual(*included_from);
    os << ",\"included_from\":{";
    os << "\"path\":";
    writeJsonString(os, vinc.path);
    os << ",\"line\":" << vinc.line;
    os << ",\"col\":" << vinc.col;
    os << '}';
  }
  os << "}\n";
}

} // namespace

// -----------------------------------------------------------------------------
// Impl
// -----------------------------------------------------------------------------

class DiagnosticEngine::Impl {
public:
  SourceManager &sm;
  std::vector<Diagnostic> diags;
  size_t error_count = 0;
  size_t warning_count = 0;

  explicit Impl(SourceManager &s) : sm(s) {}
};

DiagnosticEngine::DiagnosticEngine(SourceManager &sm)
    : impl_(std::make_unique<Impl>(sm)) {}

DiagnosticEngine::~DiagnosticEngine() = default;

DiagnosticEngine::Builder
DiagnosticEngine::report(Severity sev, SourceLocation loc, std::string msg) {
  Diagnostic d;
  d.severity = sev;
  d.loc = loc;
  d.message = std::move(msg);
  size_t idx = impl_->diags.size();
  impl_->diags.push_back(std::move(d));
  if (sev == Severity::Error) {
    ++impl_->error_count;
  } else if (sev == Severity::Warning) {
    ++impl_->warning_count;
  }
  Builder b(this, idx);
  // T068 (US3 / FR-026): auto-attach `note: included from <ancestor>`
  // entries whenever the diagnostic's file has a non-empty include
  // stack on the SourceManager. Notes are only attached on the
  // PRIMARY diagnostic (sev != Note), so chained notes don't recurse.
  if (sev != Severity::Note && loc.isValid() &&
      !impl_->sm.getIncludeStackFor(loc.file()).empty()) {
    b.addIncludedFromNotes();
  }
  return b;
}

void DiagnosticEngine::renderAll(llvm::raw_ostream &os, Format fmt) const {
  // Sort a copy at render time; the internal buffer remains in
  // emit order for cheap appends (research §4).
  std::vector<Diagnostic> sorted = impl_->diags;
  std::stable_sort(sorted.begin(), sorted.end(), diagLess);

  for (const auto &d : sorted) {
    switch (fmt) {
    case Format::Text: {
      writeTextHeaderLine(os, impl_->sm, d);
      // Emit each attached note in trailing order. Notes are NOT
      // sorted independently — they're keyed to their parent.
      for (const auto &n : d.notes) {
        writeTextHeaderLine(os, impl_->sm, n);
      }
      break;
    }
    case Format::JSON: {
      writeJsonObject(os, impl_->sm, d, /*included_from=*/nullptr);
      for (const auto &n : d.notes) {
        // For include-from notes, the loc IS the include directive;
        // surface it under both the canonical fields and
        // `included_from`.
        writeJsonObject(os, impl_->sm, n, &n.loc);
      }
      break;
    }
    }
  }
}

size_t DiagnosticEngine::numErrors() const noexcept {
  return impl_->error_count;
}

size_t DiagnosticEngine::numWarnings() const noexcept {
  return impl_->warning_count;
}

void DiagnosticEngine::clear() noexcept {
  impl_->diags.clear();
  impl_->error_count = 0;
  impl_->warning_count = 0;
}

llvm::ArrayRef<Diagnostic> DiagnosticEngine::diagnostics() const noexcept {
  return llvm::ArrayRef<Diagnostic>(impl_->diags);
}

void DiagnosticEngine::appendFixItAt(size_t index, FixItHint hint) {
  impl_->diags[index].fixits.push_back(std::move(hint));
}

void DiagnosticEngine::appendNoteAt(size_t index, Diagnostic note) {
  impl_->diags[index].notes.push_back(std::move(note));
}

SourceManager &DiagnosticEngine::sourceManager() const noexcept {
  return impl_->sm;
}

// -----------------------------------------------------------------------------
// Builder
// -----------------------------------------------------------------------------

DiagnosticEngine::Builder &
DiagnosticEngine::Builder::addFixIt(SourceRange range, std::string replacement) {
  engine_->appendFixItAt(index_, FixItHint{range, std::move(replacement)});
  return *this;
}

DiagnosticEngine::Builder &DiagnosticEngine::Builder::addIncludedFromNotes() {
  SourceManager &sm = engine_->sourceManager();
  // The diagnostic's loc tells us which file we're currently in; the
  // include stack frames trace the ancestry up to the original input.
  SourceLocation parent_loc =
      engine_->impl_->diags[index_].loc;
  if (!parent_loc.isValid()) {
    return *this;
  }
  std::vector<SourceLocation> stack =
      sm.getIncludeStackFor(parent_loc.file());
  for (const SourceLocation &include_site : stack) {
    Diagnostic note;
    note.severity = Severity::Note;
    note.loc = include_site;
    note.message = "included from";
    note.is_include_from_note = true;
    engine_->appendNoteAt(index_, std::move(note));
  }
  return *this;
}

} // namespace nsl
