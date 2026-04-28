// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// lib/Preprocess/Preprocessor.cpp — line-oriented preprocessor (T059).
//
// Architecture (research §2): a line-by-line scanner over the input
// buffer (and any `#include`'d buffers via an include stack);
// directive lines dispatch into per-directive handlers; passthrough
// lines run through `IdentSplicer` for `%IDENT%` resolution and are
// then appended to the output buffer.
//
// The output buffer contains ONLY NSL tokens + canonical `#line`
// directives (the **P12 boundary**). Per **P13** the preprocessor is
// both consumer and emitter of `#line`:
//   - Consumer: parse the directive, update the SourceManager line
//     map, advance our own cursor.
//   - Emitter: re-emit the directive in canonical form (variant 1 or
//     2 only) so the lexer/parser downstream see it.
// Additionally:
//   - At the start of an `#include`'d expansion, emit `#line 1 "<f>"`.
//   - On return from `#include`, emit a `#line N "<outer>"` to
//     re-establish the outer file's location.
//
// Cycle detection: bounded include depth at `kMaxIncludeDepth` (256).
// Conditional nesting: P9 — `#else` pairs with the most recent open
// `#if*`; mismatched directives raise FR-037 locked diagnostics.

#include "nsl/Preprocess/Preprocessor.h"

#include "DirectiveParser.h"
#include "IdentSplicer.h"
#include "PPExpression.h"
#include "nsl/Basic/Diagnostic.h"
#include "nsl/Basic/SourceLocation.h"
#include "nsl/Basic/SourceManager.h"
#include "nsl/Preprocess/HelperEvaluator.h"
#include "nsl/Preprocess/MacroTable.h"

#include "llvm/ADT/ArrayRef.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/Support/ErrorOr.h"

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <ios>
#include <memory>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

namespace nsl::preprocess {

// -----------------------------------------------------------------------------
// IncludeSearchPath
// -----------------------------------------------------------------------------

namespace {

bool fileExists(const std::string &path) {
  std::ifstream f(path, std::ios::in | std::ios::binary);
  return f.is_open();
}

/// Concatenate a directory and a relative path with a single
/// separator. Pure string-level join — no realpath, no canonicalization.
std::string joinPath(llvm::StringRef dir, llvm::StringRef name) {
  if (dir.empty()) {
    return name.str();
  }
  std::string out = dir.str();
  if (out.back() != '/') {
    out.push_back('/');
  }
  out.append(name.data(), name.size());
  return out;
}

/// Extract the directory portion of a path (everything up to and
/// including the final `/`; empty if no `/`).
std::string dirOf(llvm::StringRef path) {
  std::size_t const slash = path.rfind('/');
  if (slash == llvm::StringRef::npos) {
    return "";
  }
  return path.substr(0, slash).str();
}

} // namespace

IncludeSearchPath::IncludeSearchPath() = default;

void IncludeSearchPath::appendQuotePath(llvm::StringRef dir) {
  quote_paths_.push_back(dir.str());
}

void IncludeSearchPath::appendAnglePath(llvm::StringRef dir) {
  angle_paths_.push_back(dir.str());
}

void IncludeSearchPath::populateAngleFromEnv() {
  // Per Principle V: read NSL_INCLUDE exactly once.
  assert(!angle_env_populated_ &&
         "IncludeSearchPath::populateAngleFromEnv called more than once");
  angle_env_populated_ = true;
  const char *env = std::getenv("NSL_INCLUDE");
  if (env == nullptr) {
    return;
  }
  llvm::StringRef rest(env);
  while (!rest.empty()) {
    std::size_t const colon = rest.find(':');
    if (colon == llvm::StringRef::npos) {
      angle_paths_.push_back(rest.str());
      break;
    }
    angle_paths_.push_back(rest.substr(0, colon).str());
    rest = rest.substr(colon + 1);
  }
}

llvm::ErrorOr<std::string>
IncludeSearchPath::findQuote(llvm::StringRef filename,
                             llvm::StringRef including_dir) const {
  // 1. Including directory.
  if (!including_dir.empty()) {
    std::string p = joinPath(including_dir, filename);
    if (fileExists(p)) {
      return p;
    }
  } else {
    // Bare filename in CWD.
    std::string p = filename.str();
    if (fileExists(p)) {
      return p;
    }
  }
  // 2. -I list in registration order.
  for (const auto &dir : quote_paths_) {
    std::string p = joinPath(dir, filename);
    if (fileExists(p)) {
      return p;
    }
  }
  return std::make_error_code(std::errc::no_such_file_or_directory);
}

llvm::ErrorOr<std::string>
IncludeSearchPath::findAngle(llvm::StringRef filename) const {
  for (const auto &dir : angle_paths_) {
    std::string p = joinPath(dir, filename);
    if (fileExists(p)) {
      return p;
    }
  }
  return std::make_error_code(std::errc::no_such_file_or_directory);
}

// -----------------------------------------------------------------------------
// Preprocessor::Impl
// -----------------------------------------------------------------------------

class Preprocessor::Impl {
public:
  SourceManager &sm;
  DiagnosticEngine &diag;
  const IncludeSearchPath &search;
  MacroTable macros;
  HelperEvaluator helpers;
  PPExpression expr;
  IdentSplicer splicer;

  /// Active include frames (bottom == initial input file).
  struct Frame {
    FileID fid;
    /// Byte offset within `fid`'s buffer of the next line to read.
    std::size_t cursor{};
    /// 1-based physical line number of the current cursor position
    /// (used for correlating diagnostics with the buffer; the actual
    /// VIRTUAL line number is computed by SourceManager from
    /// `addLineDirective` calls on emit).
    std::size_t physical_line{};
    /// Stack of conditional contexts (for nested #ifdef/#if/#else).
    /// We rebuild on each frame so an `#if` started in one file
    /// can't span an `#include`.
    struct CondFrame {
      bool currently_emitting{}; ///< Are we in the active branch?
      bool any_branch_taken{}; ///< Has any branch in this if/else been emitted?
      /// True iff this is an `#if 0` style frame whose lexically
      /// enclosing branch was not emitting (suppression cascades).
      bool parent_suppressed{};
      /// The directive site (for the "unterminated #if" diagnostic).
      SourceRange opener_loc;
    };
    std::vector<CondFrame> cond_stack;
  };
  std::vector<Frame> include_stack;

  /// Output buffer (shared across all frames).
  std::string output;

  Impl(SourceManager &s, DiagnosticEngine &d, const IncludeSearchPath &sp,
       llvm::ArrayRef<std::pair<std::string, std::string>> predefined)
      : sm(s), diag(d), search(sp), helpers(d), expr(macros, helpers, d),
        splicer(macros, expr, d) {
    for (const auto &kv : predefined) {
      macros.predefine(kv.first, kv.second);
    }
  }

  // -------------------------------------------------------------------------

  Frame &top() { return include_stack.back(); }

  /// Are we currently in an "emitting" context per the conditional
  /// stack? An empty conditional stack means yes; otherwise the
  /// innermost frame's flag wins.
  [[nodiscard]] bool isEmitting(const Frame &f) const {
    if (f.cond_stack.empty()) {
      return true;
    }
    return f.cond_stack.back().currently_emitting;
  }

  /// SourceLocation for a given byte offset within the active frame.
  [[nodiscard]] SourceLocation locFor(const Frame &f,
                                      std::size_t offset) const {
    return SourceLocation::make(
        f.fid, static_cast<uint32_t>(offset >= SourceLocation::kMaxOffset
                                         ? SourceLocation::kMaxOffset - 1
                                         : offset));
  }

  // -------------------------------------------------------------------------
  // Output emission helpers
  // -------------------------------------------------------------------------

  /// Emit a canonical `#line N "FILE"` directive into the output
  /// buffer (P13 emitter rule). Always followed by a newline.
  void emitLineDirective(uint32_t line_no, llvm::StringRef path) {
    output += "#line ";
    {
      char buf[24];
      std::snprintf(buf, sizeof(buf), "%u", static_cast<unsigned>(line_no));
      output += buf;
    }
    if (!path.empty()) {
      output += " \"";
      output.append(path.data(), path.size());
      output += "\"";
    }
    output += '\n';
  }

  void emitLineForFrame(const Frame &f) {
    emitLineDirective(static_cast<uint32_t>(f.physical_line),
                      sm.getPath(f.fid));
  }

  // -------------------------------------------------------------------------
  // Line iteration over the active frame
  // -------------------------------------------------------------------------

  /// Read one physical line from the active frame. Returns true and
  /// fills `out_line`, `out_begin`, `out_end` (offsets in the active
  /// buffer); returns false at EOF. `out_had_newline` reports whether
  /// the line ended with a `\n` in the input (false iff this is the
  /// last line of a file with no trailing newline).
  bool readLine(llvm::StringRef &out_line, std::size_t &out_begin,
                std::size_t &out_end, bool &out_had_newline) {
    Frame const &f = top();
    llvm::StringRef const buf = sm.getBuffer(f.fid);
    if (f.cursor >= buf.size()) {
      return false;
    }
    out_begin = f.cursor;
    std::size_t i = f.cursor;
    while (i < buf.size() && buf[i] != '\n') {
      ++i;
    }
    std::size_t const line_end_excl_newline = i;
    out_had_newline = (i < buf.size());
    if (i < buf.size()) {
      ++i; // include the newline in `out_end`.
    }
    out_end = i;
    out_line = buf.substr(out_begin, line_end_excl_newline - out_begin);
    // Strip a trailing `\r` from CRLF lines.
    if (!out_line.empty() && out_line.back() == '\r') {
      out_line = out_line.substr(0, out_line.size() - 1);
    }
    return true;
  }

  // -------------------------------------------------------------------------
  // Directive handlers
  // -------------------------------------------------------------------------

  void handleDefine(const ParsedDirective &d, const Frame &f) {
    if (d.name.empty()) {
      diag.report(Severity::Error, locFor(f, d.line_begin_offset),
                  "missing identifier in '#define' directive");
      return;
    }
    SourceRange const loc(locFor(f, d.line_begin_offset),
                          locFor(f, d.line_end_offset));
    SourceRange previous_loc;
    bool const was_defined = macros.defined(d.name);
    macros.redefine(d.name, d.body, loc, &previous_loc);
    if (was_defined && previous_loc.isValid()) {
      // Redefinition -> attach a previous-definition note.
      auto b = diag.report(Severity::Warning, locFor(f, d.line_begin_offset),
                           "macro '" + d.name + "' redefined");
      Diagnostic note;
      note.severity = Severity::Note;
      note.loc = previous_loc.begin();
      note.message = "previous definition was here";
      diag.appendNoteAt(diag.diagnostics().size() - 1, std::move(note));
      (void)b;
    }
  }

  void handleUndef(const ParsedDirective &d, const Frame &f) {
    if (d.name.empty()) {
      diag.report(Severity::Error, locFor(f, d.line_begin_offset),
                  "missing identifier in '#undef' directive");
      return;
    }
    macros.undef(d.name);
  }

  /// Push a conditional frame. `taken_now` is whether this branch is
  /// currently active (selected). Cascading suppression: if the
  /// parent is not currently emitting, we mark the frame
  /// `parent_suppressed` so neither this branch nor its `#else` will
  /// emit until we pop back out.
  void pushCondFrame(Frame &f, bool taken_now, SourceRange opener_loc) const {
    bool const parent_suppressed = !isEmitting(f);
    Frame::CondFrame cf;
    cf.parent_suppressed = parent_suppressed;
    cf.any_branch_taken = parent_suppressed ? false : taken_now;
    cf.currently_emitting = parent_suppressed ? false : taken_now;
    cf.opener_loc = opener_loc;
    f.cond_stack.push_back(cf);
  }

  void handleIfdef(const ParsedDirective &d, Frame &f, bool negate) {
    if (d.name.empty()) {
      diag.report(Severity::Error, locFor(f, d.line_begin_offset),
                  "missing identifier in '#ifdef'/'#ifndef' directive");
      pushCondFrame(f, false,
                    SourceRange(locFor(f, d.line_begin_offset),
                                locFor(f, d.line_end_offset)));
      return;
    }
    bool const defined = macros.defined(d.name);
    bool const taken = negate ? !defined : defined;
    pushCondFrame(f, taken,
                  SourceRange(locFor(f, d.line_begin_offset),
                              locFor(f, d.line_end_offset)));
  }

  void handleIf(const ParsedDirective &d, Frame &f) {
    SourceRange const loc(locFor(f, d.line_begin_offset),
                          locFor(f, d.line_end_offset));
    // Evaluate the expression. PPExpression handles errors internally
    // (returns 0 on failure).
    PPValue const v = expr.parse(d.if_expr, locFor(f, d.line_begin_offset));
    bool const taken = v.isTruthy();
    pushCondFrame(f, taken, loc);
  }

  void handleElse(const ParsedDirective &d, Frame &f) {
    if (f.cond_stack.empty()) {
      diag.report(Severity::Error, locFor(f, d.line_begin_offset),
                  "'#else' without matching '#if' / '#ifdef' / '#ifndef'");
      return;
    }
    Frame::CondFrame &cf = f.cond_stack.back();
    if (cf.parent_suppressed) {
      // Ancestor suppressed; both branches stay off.
      cf.currently_emitting = false;
      return;
    }
    // Flip: if a then-branch was taken, the else stays off; otherwise
    // the else takes.
    if (cf.any_branch_taken) {
      cf.currently_emitting = false;
    } else {
      cf.currently_emitting = true;
      cf.any_branch_taken = true;
    }
  }

  void handleEndif(const ParsedDirective &d, Frame &f) {
    if (f.cond_stack.empty()) {
      diag.report(Severity::Error, locFor(f, d.line_begin_offset),
                  "'#endif' without matching '#if' / '#ifdef' / '#ifndef'");
      return;
    }
    f.cond_stack.pop_back();
  }

  /// Parse and consume a `#line` directive. The `operand` may be
  /// variant 1 (`<num>`), variant 2 (`<num> "<file>"`), or variant 3
  /// (anything-else; macro-expanded then re-parsed).
  ///
  /// On success: updates the SourceManager line map, updates the
  /// active frame's line cursor, and emits a canonical `#line` into
  /// the output buffer.
  void handleLine(const ParsedDirective &d, Frame &f) {
    // First splice any %IDENT% references in the operand (variant 3
    // covers macro-expansion of the directive's tail; we do this
    // unconditionally — variant 1/2 contain no `%` in their canonical
    // form, so the splice is a no-op in those cases).
    std::string expanded =
        splicer.splice(d.line_operand, locFor(f, d.line_begin_offset));

    // Re-parse expanded as `<decimal-int> [ "<filename>" ]`.
    std::size_t i = 0;
    auto skipWS = [&]() {
      while (i < expanded.size() &&
             (expanded[i] == ' ' || expanded[i] == '\t')) {
        ++i;
      }
    };
    skipWS();
    std::size_t const num_begin = i;
    while (i < expanded.size() && expanded[i] >= '0' && expanded[i] <= '9') {
      ++i;
    }
    if (i == num_begin) {
      // FR-027 locked diagnostic per parser-note N14
      // (`docs/spec/nsl_lang.ebnf:1113`). Text MUST match exactly —
      // any drift fails `test/parse/notes/n14/fail-malformed.test`.
      diag.report(Severity::Error, locFor(f, d.line_begin_offset),
                  "'#line' directive must be followed by a positive integer "
                  "(parser-note N14)");
      return;
    }
    long long line_no = 0;
    for (std::size_t k = num_begin; k < i; ++k) {
      line_no = line_no * 10 + (expanded[k] - '0');
    }
    skipWS();
    std::string virtual_path;
    if (i < expanded.size() && expanded[i] == '"') {
      // Variant 2: a quoted filename follows.
      ++i;
      std::size_t const fb = i;
      while (i < expanded.size() && expanded[i] != '"') {
        ++i;
      }
      if (i >= expanded.size()) {
        diag.report(Severity::Error, locFor(f, d.line_begin_offset),
                    "'#line' directive: unterminated filename string");
        return;
      }
      virtual_path = expanded.substr(fb, i - fb);
      ++i; // consume closing '"'
    }
    skipWS();
    if (i < expanded.size()) {
      diag.report(Severity::Error, locFor(f, d.line_begin_offset),
                  "'#line' directive: unexpected trailing content");
      // fall through — emit anyway
    }

    // Per pp.ebnf P13 + spec US2 acceptance scenario 8:
    // "the very next line of input is reported as line LINENUM"
    // So `virtual_line == line_no` (NOT `line_no + 1`).
    // `#line 100 "synth.v"` means the line AFTER the directive is
    // reported as `synth.v:100`.
    //
    // (Note: the secondary parenthetical in P13 about "LINENUM = 0 -> 1"
    // contradicts the primary text + scenario 8; we follow scenario 8.)
    auto at_off = static_cast<uint32_t>(d.line_end_offset);
    auto virtual_line = static_cast<uint32_t>(line_no);

    // Register with the SourceManager so subsequent diagnostics
    // resolve to the new (path, line) per Principle IV.
    sm.addLineDirective(SourceLocation::make(f.fid, at_off), virtual_line,
                        llvm::StringRef(virtual_path));

    // Emit the canonical form into the output. This is the ONE
    // directive that survives the seam (P13).
    emitLineDirective(static_cast<uint32_t>(line_no),
                      llvm::StringRef(virtual_path));
  }

  void handleInclude(const ParsedDirective &d, Frame &f) {
    if (d.include_filename.empty()) {
      diag.report(Severity::Error, locFor(f, d.line_begin_offset),
                  "could not find include: empty filename");
      return;
    }
    // Resolve via search path.
    llvm::ErrorOr<std::string> resolved =
        d.include_is_angle
            ? search.findAngle(d.include_filename)
            : search.findQuote(d.include_filename, dirOf(sm.getPath(f.fid)));
    if (!resolved) {
      diag.report(Severity::Error, locFor(f, d.line_begin_offset),
                  "could not find include: '" + d.include_filename + "'");
      return;
    }

    // Cycle/depth guard.
    if (include_stack.size() + 1 > Preprocessor::kMaxIncludeDepth) {
      std::string trace;
      for (auto &k : include_stack) {
        if (!trace.empty()) {
          trace += " -> ";
        }
        trace += sm.getPath(k.fid).str();
      }
      trace += " -> ";
      trace += *resolved;
      diag.report(Severity::Error, locFor(f, d.line_begin_offset),
                  "#include cycle detected: " + trace);
      return;
    }

    // Load (idempotent: same canonical path returns the same FileID).
    llvm::ErrorOr<FileID> fid_or = sm.loadFile(*resolved);
    if (!fid_or) {
      diag.report(Severity::Error, locFor(f, d.line_begin_offset),
                  "could not open include: '" + *resolved + "'");
      return;
    }
    FileID const inner = *fid_or;
    SourceLocation const include_loc = locFor(f, d.line_begin_offset);
    sm.pushIncludeFrame(include_loc, inner);

    Frame inner_frame;
    inner_frame.fid = inner;
    inner_frame.cursor = 0;
    inner_frame.physical_line = 1;
    include_stack.push_back(std::move(inner_frame));

    // Per P13: emit `#line 1 "<filename>"` at the start of the
    // included content.
    emitLineDirective(1, sm.getPath(inner));
  }

  // -------------------------------------------------------------------------
  // Top-level driver
  // -------------------------------------------------------------------------

  bool runFile(FileID input_fid) {
    // Push initial frame.
    Frame root;
    root.fid = input_fid;
    root.cursor = 0;
    root.physical_line = 1;
    include_stack.push_back(std::move(root));
    // No leading `#line 1 "<input>"` for the ROOT input — the
    // SourceManager already knows the input's path label, and emitting
    // an extra `#line` here would shift all subsequent byte offsets
    // and confuse the `-emit=tokens` golden contract (Phase 3 token
    // coordinates start at line 1, byte 0). The P13 emit-on-enter
    // rule applies only to `#include`'d files, where the path
    // transition is real.

    while (!include_stack.empty()) {
      Frame &f = top();
      llvm::StringRef line;
      std::size_t line_begin = 0;
      std::size_t line_end = 0;
      bool had_newline = false;
      if (!readLine(line, line_begin, line_end, had_newline)) {
        // EOF on this frame. Pop. Then, if we still have a parent,
        // re-establish its #line.
        bool const unterminated_if = !f.cond_stack.empty();
        if (unterminated_if) {
          for (const auto &cf : f.cond_stack) {
            diag.report(Severity::Error, cf.opener_loc.begin(),
                        "unterminated #if at end of file");
          }
        }
        FileID const popped_fid = f.fid;
        bool const was_root = (include_stack.size() == 1);
        include_stack.pop_back();
        if (!was_root) {
          // Pop the SourceManager's include frame too.
          sm.popIncludeFrame();
          // Re-establish the outer file's location: emit a `#line`
          // directive with the OUTER frame's CURRENT physical line.
          if (!include_stack.empty()) {
            Frame const &outer = top();
            emitLineForFrame(outer);
          }
        }
        (void)popped_fid;
        continue;
      }

      // We have a line. Advance cursor BEFORE classifying so any
      // recursion into included files starts cleanly.
      f.cursor = line_end;
      std::size_t const this_line_phys = f.physical_line;
      ++f.physical_line;

      ParsedDirective const pd =
          classifyLine(line, static_cast<uint32_t>(line_begin),
                       static_cast<uint32_t>(line_end));

      // Conditional gating: directives ALWAYS run (we need #else /
      // #endif even inside a suppressed branch); passthrough lines
      // are skipped if not emitting.
      const bool emitting = isEmitting(f);

      // Terminator preserves the input's "had newline" state for
      // each line. Without this, a file with no trailing newline
      // would have a phantom newline appended and shift `tk_eof`
      // to the wrong line/byte (regression in the Driver smoke
      // golden).
      const char *term = had_newline ? "\n" : "";

      switch (pd.kind) {
      case ParsedDirective::Kind::None: {
        // Passthrough line.
        if (!emitting) {
          // Emit a blank line so physical line numbers in the output
          // match the input. The downstream lexer's coordinates rely
          // on this 1:1 mapping for tokens AFTER a suppressed region.
          output += term;
          break;
        }
        std::string const spliced =
            splicer.splice(line, locFor(f, static_cast<uint32_t>(line_begin)));
        // P12 boundary check: the helper-evaluator shouldn't be
        // reachable on passthrough lines (per P6 helpers are only
        // valid inside #define / #if). Detect a residue helper call
        // pattern `_<name>(...)` where `_<name>` is a recognized
        // helper, on a passthrough line; raise the canonical
        // diagnostic.
        scanForHelperOnPassthrough(spliced, line_begin, f);
        // P7 float-at-the-seam check: scan the spliced text for any
        // float literal that survived (e.g., `2.5e3`, `1.5`).
        scanForFloatOnPassthrough(spliced, line_begin, f);
        output.append(spliced);
        output += term;
        break;
      }
      case ParsedDirective::Kind::Define:
        if (emitting) {
          handleDefine(pd, f);
        }
        output += term;
        break;
      case ParsedDirective::Kind::Undef:
        if (emitting) {
          handleUndef(pd, f);
        }
        output += term;
        break;
      case ParsedDirective::Kind::Ifdef:
        handleIfdef(pd, f, /*negate=*/false);
        output += term;
        break;
      case ParsedDirective::Kind::Ifndef:
        handleIfdef(pd, f, /*negate=*/true);
        output += term;
        break;
      case ParsedDirective::Kind::If:
        handleIf(pd, f);
        output += term;
        break;
      case ParsedDirective::Kind::Else:
        handleElse(pd, f);
        output += term;
        break;
      case ParsedDirective::Kind::Endif:
        handleEndif(pd, f);
        output += term;
        break;
      case ParsedDirective::Kind::Line:
        if (emitting) {
          handleLine(pd, f);
        } else {
          output += term;
        }
        break;
      case ParsedDirective::Kind::Include:
        if (emitting) {
          handleInclude(pd, f);
          // handleInclude either emits `#line 1 "f"` (with its own
          // newline) and pushes a frame, or fails. Either way the
          // outer line had its `\n` consumed at this point.
        } else {
          output += term;
        }
        break;
      case ParsedDirective::Kind::Unknown:
        if (emitting) {
          diag.report(Severity::Error,
                      locFor(f, static_cast<uint32_t>(line_begin)),
                      "unknown preprocessor directive: '#" + pd.name + "'");
        }
        output += term;
        break;
      }

      (void)this_line_phys;
    }

    return !diag.hasError();
  }

  // -------------------------------------------------------------------------
  // P6 / P7 seam guards
  // -------------------------------------------------------------------------

  /// Scan `text` (a passthrough line, post-`%IDENT%` splice) for any
  /// `_NAME(` pattern where `_NAME` is in the closed-set helper
  /// recognizer. Per **P6**, helpers must not appear on passthrough
  /// lines; emit the FR-037 locked diagnostic at the offending site
  /// and continue. Honors string-literal and comment skip rules.
  void scanForHelperOnPassthrough(llvm::StringRef text,
                                  std::size_t line_offset_in_buffer,
                                  const Frame &f) {
    auto isIdentStart = [](char c) {
      return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || c == '_';
    };
    auto isIdentBody = [&](char c) {
      return isIdentStart(c) || (c >= '0' && c <= '9');
    };
    std::size_t i = 0;
    while (i < text.size()) {
      char const c = text[i];
      if (c == '"') {
        ++i;
        while (i < text.size()) {
          char const d = text[i];
          if (d == '\\' && i + 1 < text.size()) {
            i += 2;
            continue;
          }
          ++i;
          if (d == '"') {
            break;
          }
        }
        continue;
      }
      if (c == '/' && i + 1 < text.size()) {
        if (text[i + 1] == '/') {
          break; // line comment to EOL
        }
        if (text[i + 1] == '*') {
          i += 2;
          while (i + 1 < text.size() &&
                 (text[i] != '*' || text[i + 1] != '/')) {
            ++i;
          }
          if (i + 1 < text.size()) {
            i += 2;
          }
          continue;
        }
      }
      if (c == '_' && (i == 0 || !isIdentBody(text[i - 1]))) {
        // Possible helper. Read the identifier.
        std::size_t const b = i;
        ++i;
        while (i < text.size() && isIdentBody(text[i])) {
          ++i;
        }
        llvm::StringRef const name = text.substr(b, i - b);
        // Skip whitespace and check for `(`.
        std::size_t j = i;
        while (j < text.size() && (text[j] == ' ' || text[j] == '\t')) {
          ++j;
        }
        if (j < text.size() && text[j] == '(' &&
            lookupHelper(name, nullptr, nullptr)) {
          // FR-037 locked diagnostic.
          std::string msg = "compile-time helper '";
          msg += name.str();
          msg += "' used outside #define / #if condition";
          diag.report(Severity::Error, locFor(f, line_offset_in_buffer + b),
                      msg);
        }
        continue;
      }
      ++i;
    }
  }

  /// Scan `text` for a float literal that survived past the seam
  /// (per P7). Honors string-literal / comment skip. The detection
  /// is conservative: a digit+`.`+digit or digit+`e`/`E` form.
  void scanForFloatOnPassthrough(llvm::StringRef text,
                                 std::size_t line_offset_in_buffer,
                                 const Frame &f) {
    auto isDigit = [](char c) { return c >= '0' && c <= '9'; };
    auto isIdentBody = [&](char c) {
      return isDigit(c) || (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
             c == '_';
    };
    std::size_t i = 0;
    while (i < text.size()) {
      char const c = text[i];
      if (c == '"') {
        ++i;
        while (i < text.size()) {
          char const d = text[i];
          if (d == '\\' && i + 1 < text.size()) {
            i += 2;
            continue;
          }
          ++i;
          if (d == '"') {
            break;
          }
        }
        continue;
      }
      if (c == '/' && i + 1 < text.size()) {
        if (text[i + 1] == '/') {
          break;
        }
        if (text[i + 1] == '*') {
          i += 2;
          while (i + 1 < text.size() &&
                 (text[i] != '*' || text[i + 1] != '/')) {
            ++i;
          }
          if (i + 1 < text.size()) {
            i += 2;
          }
          continue;
        }
      }
      // Float must begin with a digit (NSL has no leading-`.` form on
      // passthrough lines — those exist only inside `#if`/`#define`
      // expressions). The pattern `<digits>.<digits>` or `<digits>e+-?<digits>`
      // is the seam violation we hunt for.
      if (isDigit(c) && (i == 0 || !isIdentBody(text[i - 1]))) {
        std::size_t const b = i;
        bool is_float = false;
        while (i < text.size() && isDigit(text[i])) {
          ++i;
        }
        if (i < text.size() && text[i] == '.' && i + 1 < text.size() &&
            isDigit(text[i + 1])) {
          is_float = true;
          ++i;
          while (i < text.size() && isDigit(text[i])) {
            ++i;
          }
        }
        if (i < text.size() && (text[i] == 'e' || text[i] == 'E')) {
          // Exponent makes it a float, but only if the following byte
          // is a digit or sign + digit. NSL hex literals like `0xFE`
          // would otherwise trip this — but those are already past
          // the leading digit run because of the `0x` prefix scan.
          std::size_t k = i + 1;
          if (k < text.size() && (text[k] == '+' || text[k] == '-')) {
            ++k;
          }
          if (k < text.size() && isDigit(text[k])) {
            is_float = true;
            i = k;
            while (i < text.size() && isDigit(text[i])) {
              ++i;
            }
          }
        }
        if (is_float) {
          diag.report(Severity::Error, locFor(f, line_offset_in_buffer + b),
                      "float literal cannot cross the preprocessor seam");
        }
        continue;
      }
      ++i;
    }
  }
};

// -----------------------------------------------------------------------------
// Preprocessor public API
// -----------------------------------------------------------------------------

Preprocessor::Preprocessor(
    SourceManager &sm, DiagnosticEngine &diag, const IncludeSearchPath &search,
    llvm::ArrayRef<std::pair<std::string, std::string>> predefined_macros)
    : impl_(std::make_unique<Impl>(sm, diag, search, predefined_macros)) {}

Preprocessor::~Preprocessor() = default;

llvm::ErrorOr<std::string> Preprocessor::run(FileID input_fid) {
  bool const ok = impl_->runFile(input_fid);
  if (!ok) {
    return std::make_error_code(std::errc::invalid_argument);
  }
  return std::move(impl_->output);
}

} // namespace nsl::preprocess
