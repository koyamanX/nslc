// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// lib/LSP/Logger.h — stderr-only plain-text logger for `nsl-lsp`.
// Per Clarifications session 2026-05-05 Q4 → Option A and
// FR-020d–FR-020g: stderr only, no LSP `window/logMessage`,
// `NSL_LSP_LOG_LEVEL` env-var knob (`error`/`warn`/`info`/`debug`,
// default `warn`).
//
// **Specification anchors**:
//   - `specs/010-t3-lsp-skeleton/contracts/lsp-protocol.contract.md` §7
//   - `specs/010-t3-lsp-skeleton/data-model.md` §2.7
//
// Format: `<ISO-8601-UTC-second-precision> <LEVEL> <message>\n`,
// one record per line, no embedded newlines (FR-020d).
//
// **No source-content logging at any level** (FR-020f / contract
// §7.5): callers MUST NOT pass `didOpen.text` or
// `didChange.contentChanges[0].text` payloads to `Logger::log`.
// URIs and document versions are permitted.

#ifndef NSL_LSP_LOGGER_H
#define NSL_LSP_LOGGER_H

#include "llvm/ADT/StringRef.h"
#include <cstdint>

namespace nsl {
namespace lsp {

enum class LogLevel : uint8_t {
  Error = 0,
  Warn = 1,
  Info = 2,
  Debug = 3,
};

class Logger {
public:
  /// Read `NSL_LSP_LOG_LEVEL` from the environment, parse, and
  /// initialize the global level. Per FR-020e, an invalid value
  /// MUST cause the process to exit non-zero with a stderr message
  /// identifying the bad value. Permitted values (case-insensitive):
  /// `error` / `warn` / `info` / `debug`. Default when unset:
  /// `warn`. Calls `std::exit(1)` on invalid value.
  ///
  /// Idempotent: a second call replaces the level.
  static void initFromEnv();

  /// Set the level explicitly (used by unit tests; the production
  /// path uses `initFromEnv`).
  static void setLevel(LogLevel min);

  /// Read the current minimum level.
  static LogLevel level();

  /// Emit one record. Drops if `lvl` is above the configured
  /// minimum. Embedded newlines in `msg` are escaped to the
  /// two-character literal `\n` per contract §7.2.
  static void log(LogLevel lvl, llvm::StringRef msg);
};

#define NSL_LSP_LOG_ERROR(msg) ::nsl::lsp::Logger::log(::nsl::lsp::LogLevel::Error, (msg))
#define NSL_LSP_LOG_WARN(msg)  ::nsl::lsp::Logger::log(::nsl::lsp::LogLevel::Warn,  (msg))
#define NSL_LSP_LOG_INFO(msg)  ::nsl::lsp::Logger::log(::nsl::lsp::LogLevel::Info,  (msg))
#define NSL_LSP_LOG_DEBUG(msg) ::nsl::lsp::Logger::log(::nsl::lsp::LogLevel::Debug, (msg))

} // namespace lsp
} // namespace nsl

#endif // NSL_LSP_LOGGER_H
