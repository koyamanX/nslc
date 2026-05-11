// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// lib/LSP/DiagnosticMapper.cpp — diagnostic-seam impl.
//
// Per `contracts/diagnostic-mapping.contract.md` §1–§6:
//   - severity: nsl::Severity → LSP severity (Error→1, Warning→2,
//                                                Note→3)
//   - range:    SourceLocation → LSP Range via SourceManager +
//                                  byteToUtf16Column (R6)
//   - code:     extracted from the trailing `(S<NN>)` /
//                `(N<NN>)` / `(P<NN>)` suffix that the constraint
//                files embed in the message text (per
//                `lib/Sema/Constraints/S*.cpp` precedent — e.g.,
//                `S10_GenerateVarInteger.cpp:36` ends with
//                `"identifier (S10)"`)
//   - source:   message-prefix heuristic per contract §4
//   - message:  carried verbatim from `nsl::Diagnostic.message`
//   - relatedInformation: built from `nsl::Diagnostic.notes` per
//                         contract §5

#include "DiagnosticMapper.h"

#include "PositionEncoding.h"
#include "nsl/Basic/Diagnostic.h"
#include "nsl/Basic/SourceLocation.h"
#include "nsl/Basic/SourceManager.h"

#include "llvm/ADT/SmallString.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/Support/raw_ostream.h"

#include <algorithm>
#include <string>

namespace nsl::lsp {

namespace {

int severityToLsp(nsl::Severity s) {
  switch (s) {
  case nsl::Severity::Error:
    return 1;
  case nsl::Severity::Warning:
    return 2;
  case nsl::Severity::Note:
    return 3;
  }
  return 1;
}

// Extract a stable code string from the message's trailing
// `(S<NN>)` / `(N<NN>)` / `(P<NN>)` suffix. Returns an empty
// optional when no suffix matches (the diagnostic is treated as
// origin-tagged via §4 only).
std::string extractCode(llvm::StringRef msg) {
  msg = msg.rtrim();
  if (msg.size() < 4 || msg.back() != ')')
    return {};
  auto open = msg.rfind('(');
  if (open == llvm::StringRef::npos)
    return {};
  llvm::StringRef inside = msg.substr(open + 1, msg.size() - open - 2);
  if (inside.empty())
    return {};
  char prefix = inside[0];
  if (prefix != 'S' && prefix != 'N' && prefix != 'P')
    return {};
  llvm::StringRef tail = inside.drop_front();
  unsigned n = 0;
  if (tail.consumeInteger(10, n) || !tail.empty())
    return {};
  // Zero-pad to two digits per contract §1: S01, S02, ..., S29.
  llvm::SmallString<8> buf;
  llvm::raw_svector_ostream os(buf);
  os << prefix;
  if (n < 10)
    os << '0';
  os << n;
  return std::string(buf);
}

// Heuristic source-tag per contract §4. We don't have an explicit
// origin field on nsl::Diagnostic; we infer from the embedded `Sn`
// suffix (Sema), the `(N<NN>)` suffix (parse note), the `(P<NN>)`
// suffix (preprocess note), or by scanning the message text for
// preprocessor / parse markers.
llvm::StringRef inferSource(llvm::StringRef msg, llvm::StringRef code) {
  if (!code.empty()) {
    if (code.starts_with("S"))
      return "nsl-sema";
    if (code.starts_with("N"))
      return "nsl-parse";
    if (code.starts_with("P"))
      return "nsl-preprocess";
  }
  // Fall back on message-content heuristic. Preprocessor diagnostics
  // commonly mention `#`-directives, `%IDENT%` macros, or include-
  // resolution failures (`could not find include: '...'`).
  if (msg.contains("#include") || msg.contains("#define") ||
      msg.contains("#if") || msg.contains("%") ||
      msg.contains("preprocessor") || msg.contains("could not find include") ||
      msg.contains("include:") || msg.contains("macro"))
    return "nsl-preprocess";
  if (msg.contains("expected") || msg.contains("unexpected") ||
      msg.contains("missing"))
    return "nsl-parse";
  // Default — Sema/resolution diagnostics like `unresolved name '...'`
  // (per the M3 contract's last row).
  return "nsl-sema";
}

llvm::json::Value buildPosition(uint32_t line, uint32_t character) {
  return llvm::json::Object{
      {"character", static_cast<int64_t>(character)},
      {"line", static_cast<int64_t>(line)},
  };
}

llvm::json::Value buildRange(const nsl::SourceManager &sm,
                             nsl::SourceLocation loc, std::size_t length) {
  // Resolve to virtual (post-#line) coordinates per Principle IV
  // — diagnostics point to user-visible source.
  auto vloc = sm.resolveVirtual(loc);
  uint32_t zero_line = vloc.line == 0 ? 0 : vloc.line - 1;
  uint32_t zero_col_byte = vloc.col == 0 ? 0 : vloc.col - 1;

  // Convert byte column to UTF-16 code-unit column. ASCII-only
  // source (audited-corpus norm) short-circuits per R6.
  llvm::StringRef line_text = sm.getLine(loc);
  uint32_t utf16_start = byteToUtf16Column(line_text, zero_col_byte);
  uint32_t utf16_end =
      length > 0 ? byteToUtf16Column(line_text, zero_col_byte + length)
                 : utf16_start;

  return llvm::json::Object{
      {"end", buildPosition(zero_line, utf16_end)},
      {"start", buildPosition(zero_line, utf16_start)},
  };
}

} // namespace

llvm::json::Value toLspDiagnostic(const nsl::Diagnostic &d,
                                  const nsl::SourceManager &sm) {
  llvm::StringRef msg(d.message);
  std::string code = extractCode(msg);
  llvm::StringRef source = inferSource(msg, code);

  llvm::json::Object obj{
      {"range", buildRange(sm, d.loc, /*length=*/0)},
      {"severity", severityToLsp(d.severity)},
  };
  if (!code.empty())
    obj["code"] = code;
  obj["source"] = source.str();
  obj["message"] = d.message;

  // relatedInformation per contract §5.
  if (!d.notes.empty()) {
    llvm::json::Array related;
    for (const auto &note : d.notes) {
      auto vloc = sm.resolveVirtual(note.loc);
      uint32_t zline = vloc.line == 0 ? 0 : vloc.line - 1;
      uint32_t zcol = vloc.col == 0 ? 0 : vloc.col - 1;
      llvm::StringRef line_text = sm.getLine(note.loc);
      uint32_t utf16 = byteToUtf16Column(line_text, zcol);
      related.emplace_back(llvm::json::Object{
          {"location",
           llvm::json::Object{
               {"range",
                llvm::json::Object{
                    {"end", buildPosition(zline, utf16)},
                    {"start", buildPosition(zline, utf16)},
                }},
               {"uri", std::string("file://") + vloc.path.str()},
           }},
          {"message", note.message},
      });
    }
    obj["relatedInformation"] = std::move(related);
  }

  return llvm::json::Value(std::move(obj));
}

llvm::json::Array toLspDiagnosticArray(llvm::ArrayRef<nsl::Diagnostic> diags,
                                       const nsl::SourceManager &sm) {
  // Source diagnostics already arrive in their emit order. Sort by
  // (line, character, severity) per contract §6.
  std::vector<llvm::json::Value> mapped;
  mapped.reserve(diags.size());
  for (const auto &d : diags)
    mapped.emplace_back(toLspDiagnostic(d, sm));

  std::sort(mapped.begin(), mapped.end(),
            [](const llvm::json::Value &a, const llvm::json::Value &b) {
              const auto *aro =
                  a.getAsObject()->getObject("range")->getObject("start");
              const auto *bro =
                  b.getAsObject()->getObject("range")->getObject("start");
              auto al = aro->getInteger("line").value_or(0);
              auto ac = aro->getInteger("character").value_or(0);
              auto bl = bro->getInteger("line").value_or(0);
              auto bc = bro->getInteger("character").value_or(0);
              if (al != bl)
                return al < bl;
              if (ac != bc)
                return ac < bc;
              auto as = a.getAsObject()->getInteger("severity").value_or(0);
              auto bs = b.getAsObject()->getInteger("severity").value_or(0);
              return as < bs;
            });

  llvm::json::Array out;
  out.reserve(mapped.size());
  for (auto &v : mapped)
    out.emplace_back(std::move(v));
  return out;
}

} // namespace nsl::lsp
