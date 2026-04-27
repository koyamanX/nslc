// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// lib/Driver/EmitTokens.cpp — `nslc -emit=tokens` Phase 3 (US1) form.
//
// Loads the input file via `SourceManager`, runs the lexer over raw
// bytes (no preprocess yet — Phase 4 / T061 wires the preprocessor),
// and prints the buffered token stream to stdout in the canonical
// format from
// `specs/002-m1-lex-preprocess/contracts/nslc-emit-tokens.contract.md`.
//
// Buffering note: tokens are accumulated into a `std::vector<Token>`
// before any byte is written to stdout. Per the contract "No partial
// token output is printed on error" — buffering is the mechanism.
// On a diagnostic-bearing run the diagnostics flush to stderr and
// stdout receives nothing (exit code 1).
//
// SPDX-locked diagnostic strings (FR-037) live in
// `lib/Lex/Lexer.cpp` (`unterminated string literal`); this file
// only ferries them through `DiagnosticEngine::renderAll`.

#include "nsl/Driver/EmitTokens.h"

#include "nsl/Basic/Diagnostic.h"
#include "nsl/Basic/SourceLocation.h"
#include "nsl/Basic/SourceManager.h"
#include "nsl/Lex/Lexer.h"
#include "nsl/Lex/Token.h"

#include <cstdint>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

#include "llvm/ADT/StringRef.h"
#include "llvm/Support/ErrorOr.h"
#include "llvm/Support/raw_ostream.h"

namespace nsl::driver {

namespace {

/// Escape `s` for the `<spelling>` field per the contract: tabs,
/// newlines, backslashes, and double-quotes become C-style escapes.
/// Other bytes pass through verbatim (the contract does not request
/// arbitrary non-printable escaping at M1).
std::string escapeForTokenStream(llvm::StringRef s) {
  std::string out;
  out.reserve(s.size());
  for (unsigned char c : s) {
    switch (c) {
    case '\t':
      out += "\\t";
      break;
    case '\n':
      out += "\\n";
      break;
    case '\\':
      out += "\\\\";
      break;
    case '"':
      out += "\\\"";
      break;
    default:
      out.push_back(static_cast<char>(c));
      break;
    }
  }
  return out;
}

/// Render the `[Z,X,U]` flags column. Only the three numeric flags
/// are exposed at M1; the contract reserves additional flag names
/// for later milestones.
std::string renderFlags(uint16_t flags) {
  std::string out = "[";
  bool first = true;
  auto add = [&](const char *name) {
    if (!first) {
      out += ",";
    }
    out += name;
    first = false;
  };
  if (flags & Token::NF_HasZ) {
    add("Z");
  }
  if (flags & Token::NF_HasX) {
    add("X");
  }
  if (flags & Token::NF_HasU) {
    add("U");
  }
  out += "]";
  return out;
}

} // namespace

int emitTokens(llvm::StringRef input_path, const EmitTokensOptions &opts,
               llvm::raw_ostream &os, llvm::raw_ostream &err) {
  SourceManager sm;
  DiagnosticEngine diag(sm);

  // -I and -D plumbing is accepted at M1 but unused until Phase 4
  // wires the preprocessor (T061). Reference the fields once so the
  // compiler doesn't warn about unused members.
  (void)opts.include_paths;
  (void)opts.predefined_macros;

  llvm::ErrorOr<FileID> fid_or = sm.loadFile(input_path);
  if (!fid_or) {
    err << "could not open " << input_path << ": "
        << fid_or.getError().message() << "\n";
    return 3;
  }
  FileID fid = *fid_or;

  // Phase 3: no preprocess. Lex raw bytes.
  Lexer lexer(sm, fid, diag);

  // Buffer all tokens before any stdout write — partial output on
  // error is forbidden by the contract.
  std::vector<Token> tokens;
  for (;;) {
    Token t = lexer.next();
    tokens.push_back(t);
    if (t.kind() == TokenKind::tk_eof) {
      break;
    }
  }

  if (diag.hasError()) {
    diag.renderAll(err, opts.diagnostic_json
                            ? DiagnosticEngine::Format::JSON
                            : DiagnosticEngine::Format::Text);
    return 1;
  }

  for (const Token &t : tokens) {
    auto phys = sm.getLineCol(t.range().begin());
    auto virt = sm.resolveVirtual(t.range().begin());
    llvm::StringRef path = sm.getPath(t.range().begin().file());

    os << toString(t.kind()) << '\t' << escapeForTokenStream(t.spelling())
       << '\t' << path << ':' << phys.first << ':' << phys.second << ':'
       << t.range().begin().offset() << '\t' << virt.path << ':' << virt.line
       << ':' << virt.col << '\t' << renderFlags(t.flags()) << '\n';
  }

  // Even on success we render any non-error diagnostics (warnings /
  // notes) to stderr; routing matches diagnostic-output.contract.md.
  if (diag.numWarnings() > 0) {
    diag.renderAll(err, opts.diagnostic_json
                            ? DiagnosticEngine::Format::JSON
                            : DiagnosticEngine::Format::Text);
  }
  return 0;
}

} // namespace nsl::driver
