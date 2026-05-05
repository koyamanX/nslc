// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// test/lsp/diagnostics_test.cpp — Phase 3 / US1 diagnostic-seam
// integration tests (T057-T066, leaner subset). Per
// `specs/010-t3-lsp-skeleton/contracts/lsp-test-harness.contract.md`
// §4 + `lsp-protocol.contract.md` §3.
//
// Strict-TDD red state captured in `${TMPDIR:-/tmp}/t3-us1-red.txt`
// before the matching implementation tasks (T068-T072) land.

#include "LspSession.h"

#include "llvm/Support/JSON.h"
#include "llvm/Support/raw_ostream.h"

#include <algorithm>
#include <chrono>
#include <fstream>
#include <gtest/gtest.h>
#include <sstream>
#include <string>
#include <thread>

using namespace nsl::lsp::test;
using namespace std::chrono_literals;

namespace {

std::string readFixture(const std::string &name) {
  // Path baked at compile time per test/lsp/CMakeLists.txt's
  // `target_compile_definitions(... NSL_LSP_FIXTURES_DIR=...)`.
#ifndef NSL_LSP_FIXTURES_DIR
#error "NSL_LSP_FIXTURES_DIR must be defined by CMake"
#endif
  std::ifstream f(std::string(NSL_LSP_FIXTURES_DIR) + "/" + name);
  std::stringstream ss;
  ss << f.rdbuf();
  return ss.str();
}

void initialize(LspSession &s) {
  int64_t id = s.sendRequest("initialize", llvm::json::Object{});
  ASSERT_TRUE(s.waitForResponse(id).has_value());
  s.sendNotification("initialized", llvm::json::Object{});
}

void didOpen(LspSession &s, llvm::StringRef uri, int version,
              llvm::StringRef text) {
  s.sendNotification("textDocument/didOpen",
                      llvm::json::Object{
                          {"textDocument", llvm::json::Object{
                                                {"uri", uri.str()},
                                                {"languageId", "nsl"},
                                                {"version", version},
                                                {"text", text.str()},
                                            }},
                      });
}

const llvm::json::Array *getDiagnosticsArray(const llvm::json::Value &env) {
  auto *obj = env.getAsObject();
  if (!obj) return nullptr;
  auto *params = obj->getObject("params");
  if (!params) return nullptr;
  return params->getArray("diagnostics");
}

} // namespace

TEST(DiagnosticsSuite, EmptyArrayOnClean) {
  LspSession s({.nsl_lsp_log_level = "warn"});
  initialize(s);
  std::string text = readFixture("clean_module.nsl");
  ASSERT_FALSE(text.empty());
  didOpen(s, "file:///clean.nsl", 1, text);

  auto diag = s.waitForDiagnostics();
  ASSERT_TRUE(diag.has_value());
  const auto *arr = getDiagnosticsArray(*diag);
  ASSERT_NE(arr, nullptr);
  EXPECT_EQ(arr->size(), 0u) << "clean module should produce zero diagnostics";

  s.doShutdownExit();
  EXPECT_EQ(s.exitCode(), 0);
}

TEST(DiagnosticsSuite, SingleS01) {
  LspSession s({.nsl_lsp_log_level = "warn"});
  initialize(s);
  std::string text = readFixture("s01_double_underscore.nsl");
  ASSERT_FALSE(text.empty());
  didOpen(s, "file:///s01.nsl", 1, text);

  auto diag = s.waitForDiagnostics();
  ASSERT_TRUE(diag.has_value());
  const auto *arr = getDiagnosticsArray(*diag);
  ASSERT_NE(arr, nullptr);
  ASSERT_EQ(arr->size(), 1u);

  const auto *d = (*arr)[0].getAsObject();
  ASSERT_NE(d, nullptr);
  EXPECT_EQ(d->getString("code").value_or(""), "S01");
  EXPECT_EQ(d->getInteger("severity").value_or(0), 1); // Error
  EXPECT_EQ(d->getString("source").value_or(""), "nsl-sema");
  // Message contains the frozen text fragment (mapper preserves it).
  auto msg = d->getString("message").value_or("");
  EXPECT_NE(msg.find("identifier may not contain"), std::string::npos)
      << "message: " << msg.str();

  s.doShutdownExit();
  EXPECT_EQ(s.exitCode(), 0);
}

TEST(DiagnosticsSuite, SortOrder_LineThenColumn) {
  LspSession s({.nsl_lsp_log_level = "warn"});
  initialize(s);
  std::string text = readFixture("two_errors_same_line.nsl");
  ASSERT_FALSE(text.empty());
  didOpen(s, "file:///two.nsl", 1, text);

  auto diag = s.waitForDiagnostics();
  ASSERT_TRUE(diag.has_value());
  const auto *arr = getDiagnosticsArray(*diag);
  ASSERT_NE(arr, nullptr);
  ASSERT_EQ(arr->size(), 2u);

  // Line ascending: (*arr)[0].range.start.line <= (*arr)[1].range.start.line
  auto extractStart = [](const llvm::json::Value &d) {
    const auto *o = d.getAsObject()->getObject("range")->getObject("start");
    return std::pair<int64_t, int64_t>{
        o->getInteger("line").value_or(-1),
        o->getInteger("character").value_or(-1)};
  };
  auto a = extractStart((*arr)[0]);
  auto b = extractStart((*arr)[1]);
  EXPECT_TRUE(a < b) << "expected (a.line, a.col) < (b.line, b.col); "
                        << "got (" << a.first << "," << a.second
                        << ") vs (" << b.first << "," << b.second << ")";

  s.doShutdownExit();
  EXPECT_EQ(s.exitCode(), 0);
}

TEST(DiagnosticsSuite, ParseError) {
  LspSession s({.nsl_lsp_log_level = "warn"});
  initialize(s);
  std::string text = readFixture("parse_error_missing_brace.nsl");
  ASSERT_FALSE(text.empty());
  didOpen(s, "file:///pe.nsl", 1, text);

  auto diag = s.waitForDiagnostics();
  ASSERT_TRUE(diag.has_value());
  const auto *arr = getDiagnosticsArray(*diag);
  ASSERT_NE(arr, nullptr);
  ASSERT_GE(arr->size(), 1u);

  // At least one diagnostic with source = "nsl-parse".
  bool found_parse_source = false;
  for (const auto &d : *arr) {
    auto src = d.getAsObject()->getString("source").value_or("");
    if (src == "nsl-parse") { found_parse_source = true; break; }
  }
  EXPECT_TRUE(found_parse_source)
      << "expected at least one diagnostic with source = nsl-parse";

  s.doShutdownExit();
  EXPECT_EQ(s.exitCode(), 0);
}

TEST(DiagnosticsSuite, Determinism_TwoRunsByteIdentical) {
  // SC-003: two runs over identical input produce byte-identical
  // publishDiagnostics payloads.
  auto runOnce = [&]() -> std::string {
    LspSession s({.nsl_lsp_log_level = "warn"});
    initialize(s);
    std::string text = readFixture("s01_double_underscore.nsl");
    didOpen(s, "file:///d.nsl", 1, text);
    auto diag = s.waitForDiagnostics();
    EXPECT_TRUE(diag.has_value());
    std::string out;
    llvm::raw_string_ostream os(out);
    if (diag.has_value()) os << *diag;
    s.doShutdownExit();
    return out;
  };
  std::string a = runOnce();
  std::string b = runOnce();
  EXPECT_EQ(a, b);
}

namespace {

void didChange(LspSession &s, llvm::StringRef uri, int version,
                llvm::StringRef text) {
  s.sendNotification("textDocument/didChange",
                      llvm::json::Object{
                          {"textDocument", llvm::json::Object{
                                                {"uri", uri.str()},
                                                {"version", version},
                                            }},
                          {"contentChanges", llvm::json::Array{
                                                  llvm::json::Object{
                                                      {"text", text.str()},
                                                  },
                                              }},
                      });
}

void didClose(LspSession &s, llvm::StringRef uri) {
  s.sendNotification("textDocument/didClose",
                      llvm::json::Object{
                          {"textDocument", llvm::json::Object{
                                                {"uri", uri.str()},
                                            }},
                      });
}

} // namespace

TEST(DiagnosticsSuite, EditClearsResolvedDiagnostic) {
  // FR-012 / US2: open with an error → see diagnostic; edit to fix
  // → next publishDiagnostics has empty array.
  LspSession s({.nsl_lsp_log_level = "warn"});
  initialize(s);

  std::string err = readFixture("s01_double_underscore.nsl");
  std::string clean = readFixture("clean_module.nsl");
  ASSERT_FALSE(err.empty());
  ASSERT_FALSE(clean.empty());

  didOpen(s, "file:///e.nsl", 1, err);
  auto diag1 = s.waitForDiagnostics();
  ASSERT_TRUE(diag1.has_value());
  ASSERT_EQ(getDiagnosticsArray(*diag1)->size(), 1u);

  didChange(s, "file:///e.nsl", 2, clean);
  auto diag2 = s.waitForDiagnostics();
  ASSERT_TRUE(diag2.has_value());
  EXPECT_EQ(getDiagnosticsArray(*diag2)->size(), 0u);

  s.doShutdownExit();
  EXPECT_EQ(s.exitCode(), 0);
}

TEST(DiagnosticsSuite, EditIntroducesError) {
  // Inverse of EditClearsResolvedDiagnostic — open clean, edit to
  // introduce an error.
  LspSession s({.nsl_lsp_log_level = "warn"});
  initialize(s);

  std::string clean = readFixture("clean_module.nsl");
  std::string err = readFixture("s01_double_underscore.nsl");

  didOpen(s, "file:///i.nsl", 1, clean);
  auto diag1 = s.waitForDiagnostics();
  ASSERT_TRUE(diag1.has_value());
  ASSERT_EQ(getDiagnosticsArray(*diag1)->size(), 0u);

  didChange(s, "file:///i.nsl", 2, err);
  auto diag2 = s.waitForDiagnostics();
  ASSERT_TRUE(diag2.has_value());
  ASSERT_EQ(getDiagnosticsArray(*diag2)->size(), 1u);
  EXPECT_EQ(
      (*getDiagnosticsArray(*diag2))[0].getAsObject()->getString("code")
          .value_or(""),
      "S01");

  s.doShutdownExit();
  EXPECT_EQ(s.exitCode(), 0);
}

TEST(DiagnosticsSuite, DidClose_FinalEmptyDiagnostics) {
  // FR-007 / contract §2.3: didClose triggers one final empty
  // publishDiagnostics for the URI's last-known version, then
  // releases the TU.
  LspSession s({.nsl_lsp_log_level = "warn"});
  initialize(s);

  std::string err = readFixture("s01_double_underscore.nsl");
  didOpen(s, "file:///c.nsl", 1, err);
  auto diag1 = s.waitForDiagnostics();
  ASSERT_TRUE(diag1.has_value());
  ASSERT_EQ(getDiagnosticsArray(*diag1)->size(), 1u);

  didClose(s, "file:///c.nsl");
  auto diag2 = s.waitForDiagnostics();
  ASSERT_TRUE(diag2.has_value());
  EXPECT_EQ(getDiagnosticsArray(*diag2)->size(), 0u);

  s.doShutdownExit();
  EXPECT_EQ(s.exitCode(), 0);
}

TEST(DiagnosticsSuite, IncrementalChangePayload_Rejected) {
  // FR-006 / contract §2.2: server advertises Full sync; an
  // incremental payload carrying `range` is rejected with an
  // ERROR-level log + no publishDiagnostics emitted.
  LspSession s({.nsl_lsp_log_level = "warn"});
  initialize(s);

  std::string clean = readFixture("clean_module.nsl");
  didOpen(s, "file:///x.nsl", 1, clean);
  auto diag1 = s.waitForDiagnostics();
  ASSERT_TRUE(diag1.has_value());

  // Send a malformed didChange with a `range` field (incremental
  // shape).
  s.sendNotification(
      "textDocument/didChange",
      llvm::json::Object{
          {"textDocument", llvm::json::Object{
                                {"uri", "file:///x.nsl"},
                                {"version", 2},
                            }},
          {"contentChanges", llvm::json::Array{
                                  llvm::json::Object{
                                      {"range", llvm::json::Object{
                                                     {"start", llvm::json::Object{
                                                                    {"line", 0},
                                                                    {"character", 0},
                                                                }},
                                                     {"end", llvm::json::Object{
                                                                  {"line", 0},
                                                                  {"character", 0},
                                                              }},
                                                 }},
                                      {"text", "x"},
                                  },
                              }},
      });

  // No publishDiagnostics should arrive within a short window.
  auto diag2 = s.waitForDiagnostics(std::chrono::milliseconds(300));
  EXPECT_FALSE(diag2.has_value())
      << "incremental payload should be rejected; no publish expected";

  s.doShutdownExit();
  EXPECT_EQ(s.exitCode(), 0);

  // ERROR record must mention the rejection.
  std::string err_log = s.capturedStderr();
  EXPECT_NE(err_log.find("range"), std::string::npos)
      << "stderr should describe the rejection; got: " << err_log;
}

TEST(DiagnosticsSuite, StaleVersion_Ignored) {
  // Contract §2.2: didChange with version <= last-known is logged
  // WARN and ignored.
  LspSession s({.nsl_lsp_log_level = "warn"});
  initialize(s);

  std::string clean = readFixture("clean_module.nsl");
  std::string err = readFixture("s01_double_underscore.nsl");

  didOpen(s, "file:///s.nsl", 5, clean);
  auto diag1 = s.waitForDiagnostics();
  ASSERT_TRUE(diag1.has_value());

  // Send a didChange with version 3 (< 5).
  didChange(s, "file:///s.nsl", 3, err);

  // No publishDiagnostics should arrive (stale dropped).
  auto diag2 = s.waitForDiagnostics(std::chrono::milliseconds(300));
  EXPECT_FALSE(diag2.has_value())
      << "stale version should be ignored";

  s.doShutdownExit();
  EXPECT_EQ(s.exitCode(), 0);
}

// T059 (SC-002): parameterized round-trip test asserting that
// each non-constructive Sn fixture produces an LSP Diagnostic
// with the expected `code` field. Together with T058 (SingleS01)
// this materializes the SC-002 "100% of the M3 Sn round-trip"
// success criterion.
struct SnCase {
  const char *fixture;
  const char *expected_code;
};

class CodeMappingSuite : public ::testing::TestWithParam<SnCase> {};

TEST_P(CodeMappingSuite, ProducesExpectedCode) {
  const auto &c = GetParam();
  LspSession s({.nsl_lsp_log_level = "warn"});
  initialize(s);
  std::string text = readFixture(c.fixture);
  ASSERT_FALSE(text.empty()) << "missing fixture: " << c.fixture;
  didOpen(s, "file:///cm.nsl", 1, text);
  auto diag = s.waitForDiagnostics();
  ASSERT_TRUE(diag.has_value());
  const auto *arr = getDiagnosticsArray(*diag);
  ASSERT_NE(arr, nullptr);
  ASSERT_GE(arr->size(), 1u)
      << "fixture should trigger at least one Sema diagnostic";

  // At least one diagnostic carries the expected code. (Some
  // fixtures trigger additional follow-on errors — we only assert
  // the target Sn appears, not that it's the sole diagnostic.)
  bool found = false;
  for (const auto &d : *arr) {
    auto code = d.getAsObject()->getString("code").value_or("");
    if (code == c.expected_code) { found = true; break; }
  }
  EXPECT_TRUE(found) << "expected code=" << c.expected_code
                       << " in fixture " << c.fixture;

  s.doShutdownExit();
}

INSTANTIATE_TEST_SUITE_P(
    AllSn, CodeMappingSuite,
    ::testing::Values(/* values below */
        SnCase{"s01_double_underscore.nsl",            "S01"},
        SnCase{"s02_wire_with_init.nsl",               "S02"},
        SnCase{"s03_eq_on_reg.nsl",                    "S03"},
        SnCase{"s04_funcin_dummy_dir.nsl",             "S04"},
        SnCase{"s05_funcin_return_dir.nsl",            "S05"},
        SnCase{"s06_proc_arg_reg_only.nsl",            "S06"},
        SnCase{"s07_seq_outside_funcproc.nsl",         "S07"},
        SnCase{"s08_while_outside_seq.nsl",            "S08"},
        SnCase{"s09_for_var_reg.nsl",                  "S09"},
        SnCase{"s10_generate_var_integer.nsl",         "S10"},
        SnCase{"s11_state_name_proc_scoped.nsl",       "S11"},
        SnCase{"s12_partial_lhs_variable.nsl",         "S12"},
        SnCase{"s14_conditional_else_required.nsl",    "S14"},
        SnCase{"s15_slice_indices_const.nsl",          "S15"},
        SnCase{"s16_param_int_submodules.nsl",         "S16"},
        SnCase{"s17_system_task_simulation.nsl",       "S17"},
        SnCase{"s20_interface_clk_rst.nsl",            "S20"},
        SnCase{"s21_bare_finish_outside_proc.nsl",     "S21"},
        SnCase{"s22_return_outside_func.nsl",          "S22"},
        SnCase{"s25_goto_target.nsl",                  "S25"},
        SnCase{"s26_function_synonym.nsl",             "S26"},
        SnCase{"s28_first_state.nsl",                  "S28"},
        SnCase{"s29_init_block_placement.nsl",         "S29"}),
    [](const ::testing::TestParamInfo<SnCase> &info) {
      // Clean ctest name: just the expected code (S01..S29).
      return std::string(info.param.expected_code);
    });

TEST(DiagnosticsSuite, IncludeChain_FixtureLoadsAndDiagnoses) {
  // T055 fixtures (include_chain_main.nsl + helper.nslh) load via
  // NSL_INCLUDE-rooted angle-form `#include` and the helper's S1
  // violation surfaces as a diagnostic. This is the structural
  // half of T062; the relatedInformation half (FR-026) is
  // deferred — see commit narrative.
  LspSession s({.nsl_include = NSL_LSP_FIXTURES_DIR,
                  .nsl_lsp_log_level = "warn"});
  initialize(s);
  std::string text = readFixture("include_chain_main.nsl");
  ASSERT_FALSE(text.empty());
  didOpen(s, "file:///main.nsl", 1, text);

  auto diag = s.waitForDiagnostics();
  ASSERT_TRUE(diag.has_value());
  const auto *arr = getDiagnosticsArray(*diag);
  ASSERT_NE(arr, nullptr);

  // The helper's S1 violation should surface even though it
  // originates outside the open document.
  bool found_s01 = false;
  for (const auto &d : *arr) {
    auto code = d.getAsObject()->getString("code").value_or("");
    if (code == "S01") { found_s01 = true; break; }
  }
  EXPECT_TRUE(found_s01)
      << "expected an S01 diagnostic from the included helper";

  s.doShutdownExit();
  EXPECT_EQ(s.exitCode(), 0);
}

TEST(DiagnosticsSuite, PreprocessError) {
  // T064 / FR-020c: an unresolved `#include` produces a
  // preprocess-tagged diagnostic. (The frozen-Pn-code field is
  // not asserted because the preprocessor's diagnostic strings
  // don't currently carry `(P<NN>)` suffixes; the source-tag is
  // the load-bearing assertion.)
  LspSession s({.nsl_lsp_log_level = "warn"});
  initialize(s);
  std::string text = readFixture("preprocess_unresolved_include.nsl");
  ASSERT_FALSE(text.empty());
  didOpen(s, "file:///pp.nsl", 1, text);

  auto diag = s.waitForDiagnostics();
  ASSERT_TRUE(diag.has_value());
  const auto *arr = getDiagnosticsArray(*diag);
  ASSERT_NE(arr, nullptr);
  ASSERT_GE(arr->size(), 1u);

  bool found_pp = false;
  for (const auto &d : *arr) {
    auto src = d.getAsObject()->getString("source").value_or("");
    auto msg = d.getAsObject()->getString("message").value_or("");
    if (src == "nsl-preprocess" && msg.contains("include")) {
      found_pp = true;
      break;
    }
  }
  EXPECT_TRUE(found_pp)
      << "expected at least one diagnostic with source=nsl-preprocess "
         "mentioning the unresolved include";

  s.doShutdownExit();
  EXPECT_EQ(s.exitCode(), 0);
}

TEST(DiagnosticsSuite, UTF8Comment) {
  // T065 / FR-013: a fixture with a multi-byte UTF-8 comment on
  // the same line as an S1 violation. The diagnostic's
  // `range.start.character` MUST be a UTF-16 code-unit offset,
  // not a byte offset — exercises the byteToUtf16Column path
  // in DiagnosticMapper.
  LspSession s({.nsl_lsp_log_level = "warn"});
  initialize(s);
  std::string text = readFixture("utf8_comment.nsl");
  ASSERT_FALSE(text.empty());
  didOpen(s, "file:///utf.nsl", 1, text);

  auto diag = s.waitForDiagnostics();
  ASSERT_TRUE(diag.has_value());
  const auto *arr = getDiagnosticsArray(*diag);
  ASSERT_NE(arr, nullptr);
  ASSERT_GE(arr->size(), 1u);

  // Find the S01 diagnostic; assert its column reflects UTF-16
  // code-unit offset, not byte offset. S1 reports at the start
  // of the declaration (the `reg` keyword), per
  // lib/Sema/Constraints/S01_NoDoubleUnderscore.cpp using
  // `n.loc().begin()`. The fixture line is:
  //   "  /* 日本語 */ reg bad__id;"
  // Byte offsets:
  //   "  "          0..1   ( 2 bytes,  2 utf16 units)
  //   "/* "         2..4   ( 3 bytes,  3 utf16 units)
  //   "日本語"       5..13  ( 9 bytes,  3 utf16 units)  ← divergence
  //   " */ "       14..17  ( 4 bytes,  4 utf16 units)
  //   "reg" starts at byte 18 / utf16 12
  // A byte-offset bug would yield character=18; the UTF-16
  // path yields 12.
  for (const auto &d : *arr) {
    auto *o = d.getAsObject();
    auto code = o->getString("code").value_or("");
    if (code != "S01") continue;
    auto *start =
        o->getObject("range")->getObject("start");
    auto col = start->getInteger("character").value_or(-1);
    EXPECT_EQ(col, 12)
        << "expected UTF-16 code-unit column 12; got " << col
        << " (a byte-offset bug would yield 18)";
    break;
  }

  s.doShutdownExit();
  EXPECT_EQ(s.exitCode(), 0);
}

TEST(DiagnosticsSuite, OpenLatency_Under250ms_For1500Lines) {
  // SC-004 budget: didOpen → publishDiagnostics under 250 ms for
  // a 1500-line fixture (the audited-corpus-sized envelope).
  // Hardware-runner-dependent; the test gates the assertion on
  // a 4+-core x86_64 host and skips on smaller runners (matches
  // harness §4.4's slow-runner heuristic).
  unsigned hw = std::thread::hardware_concurrency();
  if (hw < 4) {
    GTEST_SKIP() << "slow runner (" << hw
                  << " cores); SC-004 budget assertion deferred";
  }

  LspSession s({.nsl_lsp_log_level = "warn"});
  initialize(s);

  std::string text = readFixture("large_file.nsl");
  ASSERT_FALSE(text.empty());
  // Sanity: confirm we're actually testing on a ≥1500-line input.
  size_t newlines = std::count(text.begin(), text.end(), '\n');
  ASSERT_GE(newlines, 1500u) << "fixture should be at least 1500 lines";

  auto t0 = std::chrono::steady_clock::now();
  didOpen(s, "file:///lat.nsl", 1, text);
  auto diag = s.waitForDiagnostics(std::chrono::milliseconds(2000));
  auto elapsed = std::chrono::steady_clock::now() - t0;
  ASSERT_TRUE(diag.has_value());

  auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed)
                .count();
  EXPECT_LT(ms, 250) << "didOpen→publishDiagnostics took " << ms
                       << " ms (SC-004 budget: 250 ms)";

  s.doShutdownExit();
  EXPECT_EQ(s.exitCode(), 0);
}

TEST(DiagnosticsSuite, RapidEdits_LatestVersionPublished) {
  // FR-008: a burst of didChanges should converge to a publish that
  // reflects the LATEST version. The version field on the final
  // publishDiagnostics matches the last didChange's version.
  LspSession s({.nsl_lsp_log_level = "warn"});
  initialize(s);

  std::string err = readFixture("s01_double_underscore.nsl");
  std::string clean = readFixture("clean_module.nsl");

  didOpen(s, "file:///r.nsl", 1, err);
  // Drain the initial publish.
  s.waitForDiagnostics();

  // Burst of 5 edits alternating between clean and error states.
  // The final version (6) is `clean`.
  didChange(s, "file:///r.nsl", 2, err);
  didChange(s, "file:///r.nsl", 3, clean);
  didChange(s, "file:///r.nsl", 4, err);
  didChange(s, "file:///r.nsl", 5, clean);
  didChange(s, "file:///r.nsl", 6, clean);

  // Drain publishes until we see version=6, OR until a longer
  // overall deadline passes. The TUScheduler may drop intermediate
  // versions per FR-008, but the version-6 publish must eventually
  // arrive (it's the latest seen). Drain takes a few hundred ms in
  // practice with a 4-wide worker pool; the 5-second deadline is
  // generous for slow runners.
  llvm::json::Value final_pub = llvm::json::Value(nullptr);
  auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
  while (std::chrono::steady_clock::now() < deadline) {
    auto pub = s.waitForDiagnostics(std::chrono::milliseconds(500));
    if (!pub.has_value()) {
      // No publish in 500 ms; if we already have version=6, done.
      if (final_pub.kind() != llvm::json::Value::Null) {
        auto *p = final_pub.getAsObject()->getObject("params");
        if (p && p->getInteger("version").value_or(-1) == 6) break;
      }
      continue;
    }
    final_pub = std::move(*pub);
  }
  auto *params = final_pub.getAsObject()->getObject("params");
  ASSERT_NE(params, nullptr);
  EXPECT_EQ(params->getInteger("version").value_or(-1), 6)
      << "latest publish must reflect the latest version received";
  EXPECT_EQ(params->getArray("diagnostics")->size(), 0u)
      << "version 6 was 'clean'; diagnostics must be empty";

  s.doShutdownExit();
  EXPECT_EQ(s.exitCode(), 0);
}
