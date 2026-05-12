// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// lib/LSP/Logger.cpp — stderr-only plain-text logger impl.

#include "Logger.h"

#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <mutex>
#include <string>

namespace nsl {
namespace lsp {

namespace {

std::atomic<LogLevel> g_level{LogLevel::Warn};
std::mutex g_write_mtx;

const char *levelLabel(LogLevel lvl) {
  switch (lvl) {
  case LogLevel::Error:
    return "ERROR";
  case LogLevel::Warn:
    return "WARN";
  case LogLevel::Info:
    return "INFO";
  case LogLevel::Debug:
    return "DEBUG";
  }
  return "?";
}

bool parseLevel(const char *s, LogLevel *out) {
  if (!s || !*s)
    return false;
  std::string lower;
  lower.reserve(8);
  for (const char *p = s; *p; ++p) {
    char c = *p;
    if (c >= 'A' && c <= 'Z')
      c = static_cast<char>(c - 'A' + 'a');
    lower.push_back(c);
  }
  if (lower == "error") {
    *out = LogLevel::Error;
    return true;
  }
  if (lower == "warn") {
    *out = LogLevel::Warn;
    return true;
  }
  if (lower == "info") {
    *out = LogLevel::Info;
    return true;
  }
  if (lower == "debug") {
    *out = LogLevel::Debug;
    return true;
  }
  return false;
}

void formatTimestamp(char *buf, std::size_t bufsz) {
  std::time_t now = std::time(nullptr);
  std::tm utc;
  // gmtime_r: POSIX, thread-safe. Linux x86_64 is the project's
  // sole supported platform per Principle IX.
  ::gmtime_r(&now, &utc);
  std::strftime(buf, bufsz, "%Y-%m-%dT%H:%M:%SZ", &utc);
}

} // namespace

void Logger::initFromEnv() {
  const char *env = std::getenv("NSL_LSP_LOG_LEVEL");
  if (!env || !*env) {
    g_level.store(LogLevel::Warn, std::memory_order_relaxed);
    return;
  }
  LogLevel parsed;
  if (!parseLevel(env, &parsed)) {
    std::fprintf(stderr,
                 "nsl-lsp: invalid NSL_LSP_LOG_LEVEL value %s "
                 "(expected one of error/warn/info/debug)\n",
                 env);
    std::exit(1);
  }
  g_level.store(parsed, std::memory_order_relaxed);
}

void Logger::setLevel(LogLevel min) {
  g_level.store(min, std::memory_order_relaxed);
}

LogLevel Logger::level() {
  return g_level.load(std::memory_order_relaxed);
}

void Logger::log(LogLevel lvl, llvm::StringRef msg) {
  if (static_cast<uint8_t>(lvl) > static_cast<uint8_t>(level()))
    return;

  // Escape embedded newlines per contract §7.2 (no embedded \n in
  // any record). Use a string buffer rather than emitting partial
  // content under the mutex.
  std::string escaped;
  escaped.reserve(msg.size() + 16);
  for (char c : msg) {
    if (c == '\n') {
      escaped.push_back('\\');
      escaped.push_back('n');
    } else if (c == '\r') {
      escaped.push_back('\\');
      escaped.push_back('r');
    } else
      escaped.push_back(c);
  }

  char ts[32];
  formatTimestamp(ts, sizeof(ts));

  std::lock_guard<std::mutex> guard(g_write_mtx);
  std::fprintf(stderr, "%s %s %s\n", ts, levelLabel(lvl), escaped.c_str());
  std::fflush(stderr);
}

} // namespace lsp
} // namespace nsl
