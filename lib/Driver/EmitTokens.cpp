// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// lib/Driver/EmitTokens.cpp — `nslc -emit=tokens` driver glue.
//
// At Phase 4 (T061) the pipeline is:
//
//   load(input)  ──►  Preprocessor::run  ──►  addBufferInMemory
//        │                                          │
//        │                                          ▼
//        │                                       Lexer::next
//        │                                          │
//        ▼                                          ▼
//    SourceManager (owns physical buffer + #line map; the
//    preprocessed buffer is registered as a synthetic in-memory
//    buffer after preprocess so the lexer scans canonical bytes
//    and downstream consumers see post-#line virtual coordinates).
//
// Buffering note: tokens are accumulated into a `std::vector<Token>`
// before any byte is written to stdout. Per
// `contracts/nslc-emit-tokens.contract.md` "No partial token output
// is printed on error" — buffering is the mechanism. On a diagnostic-
// bearing run the diagnostics flush to stderr and stdout receives
// nothing (exit code 1).
//
// SPDX-locked diagnostic strings (FR-037) live with their producing
// layer (the preprocessor / lexer); this file only ferries them
// through `DiagnosticEngine::renderAll`.

#include "nsl/Driver/EmitTokens.h"

#include "nsl/Basic/Diagnostic.h"
#include "nsl/Basic/SourceLocation.h"
#include "nsl/Basic/SourceManager.h"
#include "nsl/Lex/Lexer.h"
#include "nsl/Lex/Token.h"
#include "nsl/Preprocess/Preprocessor.h"

#include "llvm/ADT/StringRef.h"
#include "llvm/Support/ErrorOr.h"
#include "llvm/Support/raw_ostream.h"

#include <cstdint>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

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

/// Split a `-D NAME=value` argument into `(name, value)`. A bare
/// `-D NAME` (no `=`) maps to `(NAME, "1")` matching the convention
/// for `-D NAME` shorthand in C compilers.
std::pair<std::string, std::string> splitMacroDef(llvm::StringRef arg) {
  std::size_t eq = arg.find('=');
  if (eq == llvm::StringRef::npos) {
    return {arg.str(), "1"};
  }
  return {arg.substr(0, eq).str(), arg.substr(eq + 1).str()};
}

} // namespace

int emitTokens(llvm::StringRef input_path, const EmitTokensOptions &opts,
               llvm::raw_ostream &os, llvm::raw_ostream &err) {
  SourceManager sm;
  DiagnosticEngine diag(sm);

  llvm::ErrorOr<FileID> fid_or = sm.loadFile(input_path);
  if (!fid_or) {
    err << "could not open " << input_path << ": "
        << fid_or.getError().message() << "\n";
    return 3;
  }
  FileID input_fid = *fid_or;

  // Construct the include-search path. Quote-form paths from `-I`;
  // angle-form paths from NSL_INCLUDE (read once at construction per
  // Principle V).
  preprocess::IncludeSearchPath search;
  for (const auto &dir : opts.include_paths) {
    search.appendQuotePath(dir);
  }
  search.populateAngleFromEnv();

  // Predefine macros from `-D NAME=value`.
  std::vector<std::pair<std::string, std::string>> predefined;
  predefined.reserve(opts.predefined_macros.size());
  for (const auto &arg : opts.predefined_macros) {
    predefined.push_back(splitMacroDef(arg));
  }

  preprocess::Preprocessor pp(sm, diag, search, predefined);
  llvm::ErrorOr<std::string> pp_out = pp.run(input_fid);

  // If preprocessing errored (or yielded an error result), surface
  // diagnostics and return 1.
  if (diag.hasError() || !pp_out) {
    diag.renderAll(err, opts.diagnostic_json ? DiagnosticEngine::Format::JSON
                                             : DiagnosticEngine::Format::Text);
    return 1;
  }

  // Register the preprocessed buffer as a synthetic in-memory buffer.
  // The downstream lexer scans this canonical-NSL stream, while the
  // SourceManager carries the line-override map populated below from
  // the preprocessor's emitted `#line` directives.
  //
  // Why scan the output here: the preprocessor's `addLineDirective`
  // calls during preprocess-time register on the ORIGINAL input
  // FileID, but the lexer scans the SYNTHETIC buffer (a different
  // FileID). For Principle-IV virtual-location resolution to work on
  // tokens emitted by the lexer, we must replay the `#line`
  // directives onto the synthetic FileID. The output buffer is the
  // ground truth — every `#line` we want the lexer to honor is
  // present there in canonical form.
  std::string synth_path = input_path.str();
  std::vector<char> synth_bytes(pp_out->begin(), pp_out->end());
  FileID synth_fid =
      sm.addBufferInMemory(std::move(synth_path), std::move(synth_bytes));

  // Replay #line directives onto the synthetic buffer. We scan the
  // preprocessed text line by line; each `#line N "FILE"` (or
  // `#line N`) at column 0 calls addLineDirective on synth_fid.
  {
    llvm::StringRef syn = sm.getBuffer(synth_fid);
    std::size_t off = 0;
    while (off < syn.size()) {
      std::size_t line_begin = off;
      while (off < syn.size() && syn[off] != '\n') {
        ++off;
      }
      std::size_t line_end_excl = off;
      if (off < syn.size()) {
        ++off; // consume newline
      }
      llvm::StringRef line = syn.substr(line_begin, line_end_excl - line_begin);
      // Match `#line ` at column 0.
      if (!line.starts_with("#line ")) {
        continue;
      }
      std::size_t i = 6; // past "#line "
      while (i < line.size() && (line[i] == ' ' || line[i] == '\t')) {
        ++i;
      }
      if (i >= line.size() || line[i] < '0' || line[i] > '9') {
        continue;
      }
      long long ln = 0;
      while (i < line.size() && line[i] >= '0' && line[i] <= '9') {
        ln = ln * 10 + (line[i] - '0');
        ++i;
      }
      while (i < line.size() && (line[i] == ' ' || line[i] == '\t')) {
        ++i;
      }
      std::string vpath;
      if (i < line.size() && line[i] == '"') {
        ++i;
        std::size_t fb = i;
        while (i < line.size() && line[i] != '"') {
          ++i;
        }
        if (i < line.size()) {
          vpath = line.substr(fb, i - fb).str();
        }
      }
      // The override takes effect at the byte AFTER the directive's
      // trailing newline. Per pp.ebnf P13 + spec acceptance scenario
      // 8: "the very next line of input is reported as line LINENUM"
      // — so `virtual_line == ln` (NOT `ln+1`). `#line 100 "synth.v"`
      // means the line AFTER the directive is `synth.v:100`.
      auto at_off = static_cast<uint32_t>(off);
      sm.addLineDirective(SourceLocation::make(synth_fid, at_off),
                          static_cast<uint32_t>(ln), llvm::StringRef(vpath));
    }
  }

  Lexer lexer(sm, synth_fid, diag);

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
    diag.renderAll(err, opts.diagnostic_json ? DiagnosticEngine::Format::JSON
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
    diag.renderAll(err, opts.diagnostic_json ? DiagnosticEngine::Format::JSON
                                             : DiagnosticEngine::Format::Text);
  }
  return 0;
}

} // namespace nsl::driver
