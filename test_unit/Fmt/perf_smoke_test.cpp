// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// test_unit/Fmt/perf_smoke_test.cpp
//
// SC-003 perf-budget smoke test (T118 — added during /speckit-analyze
// remediation, finding C2). Constructs a synthetic 1000-line NSL
// file and asserts that `format_buffer()` (with an internal
// `--check`-equivalent comparison) completes within the perf
// budget.
//
// **Budget rationale**: spec.md SC-003 specifies "under 250 ms on
// the dev-container hardware target." We assert `< 500 ms` here —
// 2× SC-003 — to absorb CI hardware variance (slower runners,
// noisy neighbours, ASan instrumentation when enabled). A real
// perf regression typically widens latency by an order of
// magnitude, so the 2× slack catches regressions without false-
// positive flake.

#include "nsl/Basic/SourceLocation.h"
#include "nsl/Fmt/Fmt.h"

#include "llvm/ADT/StringRef.h"

#include "gtest/gtest.h"
#include <chrono>
#include <cstdint>
#include <string>

namespace {

// Build a 1000-line NSL synthetic input. One module containing 1000
// `wire foo_<n> [8];` declarations is well-formed NSL and exercises
// the directive splitter on a uniform-shape input.
std::string buildSynthetic1000Line() {
  std::string out;
  out.reserve(64 * 1024);
  out += "module synth_perf {\n";
  for (int i = 0; i < 998; ++i) {
    out += "    wire foo_";
    out += std::to_string(i);
    out += " [8];\n";
  }
  out += "}\n";
  return out;
}

TEST(FmtPerfSmokeTest, Check1000LineUnder500ms) {
  std::string source = buildSynthetic1000Line();
  // Confirm the synthetic build matches the expected line count
  // before we time anything (so a future change to buildSynthetic*
  // that drifts the count still gates SC-003 against a known
  // input size).
  std::size_t newlineCount = 0;
  for (char c : source) {
    if (c == '\n') {
      ++newlineCount;
    }
  }
  ASSERT_EQ(newlineCount, 1000u)
      << "synthetic input should be exactly 1000 lines";

  ::nsl::FileID fid(7);
  nsl::fmt::Configuration cfg = nsl::fmt::default_configuration();

  auto t0 = std::chrono::steady_clock::now();
  // `--check`-equivalent: format the buffer and compare against
  // input. At Phase 3c the formatter is still byte-passthrough
  // (LayoutPlanner pending) so the comparison succeeds; the timing
  // measures the parse + DirectiveSplitter + emission path.
  nsl::fmt::FormatResult res =
      nsl::fmt::format_buffer(llvm::StringRef(source), cfg, fid, std::nullopt);
  bool clean = (res.status == nsl::fmt::FormatResult::Status::Success) &&
               (res.formattedText == source);
  auto t1 = std::chrono::steady_clock::now();

  EXPECT_TRUE(clean) << "byte-passthrough should match the synthetic input";

  auto elapsed_ms =
      std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count();

  // SC-003 budget: 250 ms. Test budget: 500 ms (2× slack).
  EXPECT_LT(elapsed_ms, 500)
      << "format_buffer on 1000-line input took " << elapsed_ms
      << " ms; SC-003 budget is 250 ms (test budget 500 ms with 2x slack)";
}

} // namespace
