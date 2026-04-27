// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// include/nsl/Preprocess/Preprocessor.h
//
// Public surface for `nsl-preprocess` (the layer-2 library per
// `docs/design/nsl_compiler_design.md` §3 lines 132–148; FR-002).
// The preprocessor consumes raw NSL source text and emits a buffer
// containing only NSL tokens + canonical `#line` directives — the
// **P12 boundary** (`pp.ebnf` notes P12 + P13).
//
// Public-API surface intentionally kept minimal (Constitution
// Principle II): every implementation type (`MacroTable`,
// `HelperEvaluator`, etc.) lives in `lib/Preprocess/` private headers
// and is NOT part of this header. Drivers (`lib/Driver/EmitTokens.cpp`)
// + the future M2 parser see only `Preprocessor` + `IncludeSearchPath`.
//
// Determinism (Principle V): `IncludeSearchPath` reads the
// `NSL_INCLUDE` env var ONCE at construction (research §4 / contract
// "Include-search-path order"). No env-var read after construction.
//
// Cycle detection (FR-022): include depth is hard-bounded at 256
// frames (`kMaxIncludeDepth`). Exceeding raises an "include cycle
// detected" diagnostic and aborts the run with an error.

#ifndef NSL_PREPROCESS_PREPROCESSOR_H
#define NSL_PREPROCESS_PREPROCESSOR_H

#include "nsl/Basic/Diagnostic.h"
#include "nsl/Basic/SourceLocation.h"
#include "nsl/Basic/SourceManager.h"

#include "llvm/ADT/ArrayRef.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/Support/ErrorOr.h"

#include <cstddef>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace nsl::preprocess {

/// Ordered list of directories searched for `#include`. Quote-form
/// (`#include "f"`) and angle-form (`#include <f>`) maintain
/// SEPARATE lists with different semantics per **P8** + the
/// `preprocessor-seam.contract.md` "Include-search-path order"
/// section.
class IncludeSearchPath {
public:
  /// Construct an empty search path. `NSL_INCLUDE` is NOT read here
  /// — call `populateAngleFromEnv()` once to honor the env-var
  /// contract.
  IncludeSearchPath();

  /// Append a directory to the QUOTE-form path (typically `-I <dir>`
  /// from the CLI).
  void appendQuotePath(llvm::StringRef dir);

  /// Append a directory to the ANGLE-form path. Typically called by
  /// `populateAngleFromEnv()`.
  void appendAnglePath(llvm::StringRef dir);

  /// Read the `NSL_INCLUDE` environment variable (colon-separated on
  /// POSIX) ONCE and append every entry to the angle-form path. Safe
  /// to call zero or one times; calling twice is a programming error
  /// and triggers an assertion in non-NDEBUG builds.
  void populateAngleFromEnv();

  /// Resolve a quote-form `#include "filename"`. Search order:
  ///   1. `including_dir` (the directory of the file containing the
  ///      `#include` directive).
  ///   2. Each `-I` directory in registration order.
  /// Returns the resolved absolute path or an error if not found.
  [[nodiscard]] llvm::ErrorOr<std::string>
  findQuote(llvm::StringRef filename, llvm::StringRef including_dir) const;

  /// Resolve an angle-form `#include <filename>`. Search order: each
  /// entry in the `NSL_INCLUDE`-derived path list, in registration
  /// order. **`-I` is NOT consulted** for angle-form per P8.
  [[nodiscard]] llvm::ErrorOr<std::string>
  findAngle(llvm::StringRef filename) const;

private:
  std::vector<std::string> quote_paths_;
  std::vector<std::string> angle_paths_;
  bool angle_env_populated_ = false;
};

/// `Preprocessor` consumes raw NSL source and emits a buffer
/// containing only NSL tokens plus canonical `#line` directives (the
/// P12 boundary).
class Preprocessor {
public:
  /// Hard cap on include-stack depth (FR-022). Not user-configurable
  /// at M1.
  static constexpr std::size_t kMaxIncludeDepth = 256;

  /// Construct a preprocessor.
  /// @param sm                  source manager (must outlive `*this`).
  /// @param diag                diagnostic engine (must outlive `*this`).
  /// @param search              quote/angle include-search lists.
  /// @param predefined_macros   `(name, body)` pairs from `-D` flags;
  ///                            inserted before any source-defined
  ///                            macro per data-model entity 11.
  Preprocessor(
      SourceManager &sm, DiagnosticEngine &diag,
      const IncludeSearchPath &search,
      llvm::ArrayRef<std::pair<std::string, std::string>> predefined_macros);

  ~Preprocessor();

  Preprocessor(const Preprocessor &) = delete;
  Preprocessor &operator=(const Preprocessor &) = delete;

  /// Run the preprocessor over `input_fid`, returning the resulting
  /// preprocessed buffer (raw bytes). The buffer is suitable for the
  /// lexer to re-scan; per **P12** it contains ONLY NSL tokens plus
  /// canonical `#line` directives.
  ///
  /// On success returns the buffer. On preprocessor error, the
  /// diagnostic engine has the details and the returned ErrorOr
  /// contains an error code; callers SHOULD render diagnostics from
  /// `diag` (not from the returned `std::error_code` text).
  llvm::ErrorOr<std::string> run(FileID input_fid);

private:
  class Impl;
  std::unique_ptr<Impl> impl_;
};

} // namespace nsl::preprocess

#endif // NSL_PREPROCESS_PREPROCESSOR_H
