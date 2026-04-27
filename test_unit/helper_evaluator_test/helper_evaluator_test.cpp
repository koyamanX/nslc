// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// test_unit/helper_evaluator_test/helper_evaluator_test.cpp
//
// TDD fixtures for the `HelperEvaluator` (data-model entity 13)
// covering all 22 closed-set helpers from
// `include/nsl/Basic/HelperSet.def` (T005). Per tasks.md T052:
//
//   * Basic evaluation (one canonical input per helper).
//   * Integer↔real coercion (`_int(2.5)` → 2; `_real(3)` → 3.0).
//   * Kind-preserving `_abs`/`_min`/`_max` (integer→integer;
//     mixed-or-real→real) per research §5 final clause.
//   * Domain errors (`_log(0)`, `_sqrt(-1)`, `_asin(2)`) emit
//     `error: helper '_NAME' domain error: <reason>` per research
//     §10 and produce integer `0` for failsoft continuation.
//   * Arity mismatch detected at parse time (research §10 case 3).
//
// Authored RED before `lib/Preprocess/HelperEvaluator.cpp` exists;
// this suite is expected to FAIL TO LINK against the unchanged tree
// — `nsl::HelperEvaluator` symbols are not yet defined. Constitution
// Principle VIII RED-state evidence (FR-036).
//
// **Public API contract assumed by these fixtures** (the parallel
// `nsl-frontend-impl` agent shapes `lib/Preprocess/HelperEvaluator.cpp`
// to satisfy this; if the API differs at land time, the failing
// link/compile is what reveals the discrepancy and the agents
// negotiate the surface):
//
//   namespace nsl {
//   class PPValue {
//    public:
//     PPValue();                               // default = integer 0
//     static PPValue fromInt (int64_t v);
//     static PPValue fromReal(long double v);
//     bool        isInt()  const;
//     bool        isReal() const;
//     int64_t     toInt()  const;              // truncate-toward-zero on real
//     long double toReal() const;              // widen integer
//     bool        isTruthy() const;
//   };
//
//   class HelperEvaluator {
//    public:
//     HelperEvaluator(DiagnosticEngine& diag);
//     /// Evaluate a helper call by name (no leading underscore).
//     /// `args.size()` is checked against the arity in HelperSet.def.
//     /// Returns failsoft integer `0` on domain error and emits an
//     /// error diagnostic at `loc`.
//     PPValue call(llvm::StringRef name,
//                  llvm::ArrayRef<PPValue> args,
//                  SourceLocation loc);
//   };
//   } // namespace nsl

#include "nsl/Basic/Diagnostic.h"
#include "nsl/Basic/SourceLocation.h"
#include "nsl/Basic/SourceManager.h"
#include "nsl/Preprocess/HelperEvaluator.h"

#include "llvm/ADT/ArrayRef.h"
#include "llvm/ADT/StringRef.h"

#include "gtest/gtest.h"
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

using nsl::DiagnosticEngine;
using nsl::FileID;
using nsl::Severity;
using nsl::SourceLocation;
using nsl::SourceManager;
using nsl::preprocess::HelperEvaluator;
using nsl::preprocess::PPValue;

namespace {

// ---- Helpers for fixture setup ---------------------------------------

FileID makeBuf(SourceManager &sm) {
  std::vector<char> bytes{'X', '\0'};
  return sm.addBufferInMemory("synthetic.nsl", std::move(bytes));
}

nsl::SourceRange syntheticLoc(SourceManager &sm, FileID f) {
  SourceLocation b = SourceLocation::make(f, 0);
  SourceLocation e = SourceLocation::make(f, 1);
  return nsl::SourceRange(b, e);
}

// Convenience constructors for the failsoft tests below.
PPValue I(int64_t v) {
  return PPValue(v);
}
PPValue R(long double v) {
  return PPValue(v);
}

// True iff diag has at least one error-severity diagnostic whose
// message contains `needle`. (The exact wording per research §10 is
// `error: helper '_NAME' domain error: <reason>`; tests assert
// containment, not exact equality, to leave the implementer some
// freedom on the trailing reason text.)
bool hasError(const DiagnosticEngine &diag, llvm::StringRef needle) {
  for (const auto &d : diag.diagnostics()) {
    if (d.severity == Severity::Error &&
        llvm::StringRef(d.message).contains(needle)) {
      return true;
    }
  }
  return false;
}

// Assertion helper: real-valued result with a small epsilon for
// trig/log helpers where the exact long-double bit pattern is
// platform-sensitive.
::testing::AssertionResult NearReal(PPValue got, long double want,
                                    long double eps = 1e-12L) {
  if (!got.isReal()) {
    return ::testing::AssertionFailure()
           << "expected real-valued PPValue; got integer " << got.toInt();
  }
  long double g = got.toReal();
  long double diff = fabsl(g - want);
  if (diff > eps) {
    return ::testing::AssertionFailure()
           << "real value " << static_cast<double>(g) << " differs from "
           << static_cast<double>(want) << " by " << static_cast<double>(diff)
           << " (> " << static_cast<double>(eps) << ")";
  }
  return ::testing::AssertionSuccess();
}

// =============================================================
// GROUP 1 — _int / _real (coercion)
// =============================================================

TEST(HelperEvaluatorTest, IntCoercesRealToInteger_TruncTowardZero) {
  SourceManager sm;
  DiagnosticEngine diag(sm);
  FileID f = makeBuf(sm);
  HelperEvaluator h(diag);

  PPValue r1 = h.invoke("_int", {R(2.7L)}, syntheticLoc(sm, f));
  ASSERT_TRUE(r1.isInt());
  EXPECT_EQ(r1.toInt(), 2);

  // Negative — truncate TOWARD ZERO, not floor.
  PPValue r2 = h.invoke("_int", {R(-2.7L)}, syntheticLoc(sm, f));
  ASSERT_TRUE(r2.isInt());
  EXPECT_EQ(r2.toInt(), -2);
}

TEST(HelperEvaluatorTest, IntPassesIntegerThrough) {
  SourceManager sm;
  DiagnosticEngine diag(sm);
  FileID f = makeBuf(sm);
  HelperEvaluator h(diag);

  PPValue r = h.invoke("_int", {I(42)}, syntheticLoc(sm, f));
  ASSERT_TRUE(r.isInt());
  EXPECT_EQ(r.toInt(), 42);
}

TEST(HelperEvaluatorTest, RealWidensIntegerToReal) {
  SourceManager sm;
  DiagnosticEngine diag(sm);
  FileID f = makeBuf(sm);
  HelperEvaluator h(diag);

  PPValue r = h.invoke("_real", {I(3)}, syntheticLoc(sm, f));
  ASSERT_TRUE(r.isReal());
  EXPECT_EQ(r.toReal(), 3.0L);
}

TEST(HelperEvaluatorTest, RealPassesRealThrough) {
  SourceManager sm;
  DiagnosticEngine diag(sm);
  FileID f = makeBuf(sm);
  HelperEvaluator h(diag);

  PPValue r = h.invoke("_real", {R(2.5L)}, syntheticLoc(sm, f));
  EXPECT_TRUE(NearReal(r, 2.5L));
}

// =============================================================
// GROUP 1 — _pow / _sqrt (power / root)
// =============================================================

TEST(HelperEvaluatorTest, PowBasic) {
  SourceManager sm;
  DiagnosticEngine diag(sm);
  FileID f = makeBuf(sm);
  HelperEvaluator h(diag);

  PPValue r = h.invoke("_pow", {R(2.0L), R(8.0L)}, syntheticLoc(sm, f));
  EXPECT_TRUE(NearReal(r, 256.0L));
}

TEST(HelperEvaluatorTest, PowIntegerArgsWiden) {
  SourceManager sm;
  DiagnosticEngine diag(sm);
  FileID f = makeBuf(sm);
  HelperEvaluator h(diag);

  PPValue r = h.invoke("_pow", {I(2), I(8)}, syntheticLoc(sm, f));
  EXPECT_TRUE(NearReal(r, 256.0L));
}

TEST(HelperEvaluatorTest, SqrtBasic) {
  SourceManager sm;
  DiagnosticEngine diag(sm);
  FileID f = makeBuf(sm);
  HelperEvaluator h(diag);

  PPValue r = h.invoke("_sqrt", {R(16.0L)}, syntheticLoc(sm, f));
  EXPECT_TRUE(NearReal(r, 4.0L));
}

TEST(HelperEvaluatorTest, SqrtNegativeIsDomainError_FailsoftZero) {
  SourceManager sm;
  DiagnosticEngine diag(sm);
  FileID f = makeBuf(sm);
  HelperEvaluator h(diag);

  PPValue r = h.invoke("_sqrt", {R(-1.0L)}, syntheticLoc(sm, f));
  EXPECT_TRUE(hasError(diag, "domain error"))
      << "_sqrt(-1) should emit a domain-error diagnostic";
  // Failsoft path returns integer 0 per research §10.
  ASSERT_TRUE(r.isInt());
  EXPECT_EQ(r.toInt(), 0);
}

// =============================================================
// GROUP 2 — _sin / _cos / _tan
// =============================================================

TEST(HelperEvaluatorTest, SinZero) {
  SourceManager sm;
  DiagnosticEngine diag(sm);
  FileID f = makeBuf(sm);
  HelperEvaluator h(diag);

  PPValue r = h.invoke("_sin", {R(0.0L)}, syntheticLoc(sm, f));
  EXPECT_TRUE(NearReal(r, 0.0L));
}

TEST(HelperEvaluatorTest, CosZero) {
  SourceManager sm;
  DiagnosticEngine diag(sm);
  FileID f = makeBuf(sm);
  HelperEvaluator h(diag);

  PPValue r = h.invoke("_cos", {R(0.0L)}, syntheticLoc(sm, f));
  EXPECT_TRUE(NearReal(r, 1.0L));
}

TEST(HelperEvaluatorTest, TanZero) {
  SourceManager sm;
  DiagnosticEngine diag(sm);
  FileID f = makeBuf(sm);
  HelperEvaluator h(diag);

  PPValue r = h.invoke("_tan", {R(0.0L)}, syntheticLoc(sm, f));
  EXPECT_TRUE(NearReal(r, 0.0L));
}

// =============================================================
// GROUP 3 — _asin / _acos / _atan
// =============================================================

TEST(HelperEvaluatorTest, AsinZero) {
  SourceManager sm;
  DiagnosticEngine diag(sm);
  FileID f = makeBuf(sm);
  HelperEvaluator h(diag);

  PPValue r = h.invoke("_asin", {R(0.0L)}, syntheticLoc(sm, f));
  EXPECT_TRUE(NearReal(r, 0.0L));
}

TEST(HelperEvaluatorTest, AsinOutOfRangeIsDomainError_FailsoftZero) {
  SourceManager sm;
  DiagnosticEngine diag(sm);
  FileID f = makeBuf(sm);
  HelperEvaluator h(diag);

  PPValue r = h.invoke("_asin", {R(2.0L)}, syntheticLoc(sm, f));
  EXPECT_TRUE(hasError(diag, "domain error"))
      << "_asin(2) should emit a domain-error diagnostic";
  ASSERT_TRUE(r.isInt());
  EXPECT_EQ(r.toInt(), 0);
}

TEST(HelperEvaluatorTest, AcosOne) {
  SourceManager sm;
  DiagnosticEngine diag(sm);
  FileID f = makeBuf(sm);
  HelperEvaluator h(diag);

  PPValue r = h.invoke("_acos", {R(1.0L)}, syntheticLoc(sm, f));
  EXPECT_TRUE(NearReal(r, 0.0L));
}

TEST(HelperEvaluatorTest, AcosOutOfRangeIsDomainError_FailsoftZero) {
  SourceManager sm;
  DiagnosticEngine diag(sm);
  FileID f = makeBuf(sm);
  HelperEvaluator h(diag);

  PPValue r = h.invoke("_acos", {R(-2.0L)}, syntheticLoc(sm, f));
  EXPECT_TRUE(hasError(diag, "domain error"));
  ASSERT_TRUE(r.isInt());
  EXPECT_EQ(r.toInt(), 0);
}

TEST(HelperEvaluatorTest, AtanZero) {
  SourceManager sm;
  DiagnosticEngine diag(sm);
  FileID f = makeBuf(sm);
  HelperEvaluator h(diag);

  PPValue r = h.invoke("_atan", {R(0.0L)}, syntheticLoc(sm, f));
  EXPECT_TRUE(NearReal(r, 0.0L));
}

// =============================================================
// GROUP 4 — _sinh / _cosh / _tanh
// =============================================================

TEST(HelperEvaluatorTest, SinhZero) {
  SourceManager sm;
  DiagnosticEngine diag(sm);
  FileID f = makeBuf(sm);
  HelperEvaluator h(diag);

  PPValue r = h.invoke("_sinh", {R(0.0L)}, syntheticLoc(sm, f));
  EXPECT_TRUE(NearReal(r, 0.0L));
}

TEST(HelperEvaluatorTest, CoshZero) {
  SourceManager sm;
  DiagnosticEngine diag(sm);
  FileID f = makeBuf(sm);
  HelperEvaluator h(diag);

  PPValue r = h.invoke("_cosh", {R(0.0L)}, syntheticLoc(sm, f));
  EXPECT_TRUE(NearReal(r, 1.0L));
}

TEST(HelperEvaluatorTest, TanhZero) {
  SourceManager sm;
  DiagnosticEngine diag(sm);
  FileID f = makeBuf(sm);
  HelperEvaluator h(diag);

  PPValue r = h.invoke("_tanh", {R(0.0L)}, syntheticLoc(sm, f));
  EXPECT_TRUE(NearReal(r, 0.0L));
}

// =============================================================
// GROUP 5 — _log / _log10 / _exp
// =============================================================

TEST(HelperEvaluatorTest, LogOneIsZero) {
  SourceManager sm;
  DiagnosticEngine diag(sm);
  FileID f = makeBuf(sm);
  HelperEvaluator h(diag);

  PPValue r = h.invoke("_log", {R(1.0L)}, syntheticLoc(sm, f));
  EXPECT_TRUE(NearReal(r, 0.0L));
}

TEST(HelperEvaluatorTest, LogZeroIsDomainError_FailsoftZero) {
  SourceManager sm;
  DiagnosticEngine diag(sm);
  FileID f = makeBuf(sm);
  HelperEvaluator h(diag);

  PPValue r = h.invoke("_log", {R(0.0L)}, syntheticLoc(sm, f));
  EXPECT_TRUE(hasError(diag, "domain error"))
      << "_log(0) should emit a domain-error diagnostic";
  ASSERT_TRUE(r.isInt());
  EXPECT_EQ(r.toInt(), 0);
}

TEST(HelperEvaluatorTest, LogNegativeIsDomainError_FailsoftZero) {
  SourceManager sm;
  DiagnosticEngine diag(sm);
  FileID f = makeBuf(sm);
  HelperEvaluator h(diag);

  PPValue r = h.invoke("_log", {R(-1.0L)}, syntheticLoc(sm, f));
  EXPECT_TRUE(hasError(diag, "domain error"));
  ASSERT_TRUE(r.isInt());
  EXPECT_EQ(r.toInt(), 0);
}

TEST(HelperEvaluatorTest, Log10TenIsOne) {
  SourceManager sm;
  DiagnosticEngine diag(sm);
  FileID f = makeBuf(sm);
  HelperEvaluator h(diag);

  PPValue r = h.invoke("_log10", {R(10.0L)}, syntheticLoc(sm, f));
  EXPECT_TRUE(NearReal(r, 1.0L));
}

TEST(HelperEvaluatorTest, Log10ZeroIsDomainError_FailsoftZero) {
  SourceManager sm;
  DiagnosticEngine diag(sm);
  FileID f = makeBuf(sm);
  HelperEvaluator h(diag);

  PPValue r = h.invoke("_log10", {R(0.0L)}, syntheticLoc(sm, f));
  EXPECT_TRUE(hasError(diag, "domain error"));
  ASSERT_TRUE(r.isInt());
  EXPECT_EQ(r.toInt(), 0);
}

TEST(HelperEvaluatorTest, ExpZero) {
  SourceManager sm;
  DiagnosticEngine diag(sm);
  FileID f = makeBuf(sm);
  HelperEvaluator h(diag);

  PPValue r = h.invoke("_exp", {R(0.0L)}, syntheticLoc(sm, f));
  EXPECT_TRUE(NearReal(r, 1.0L));
}

// =============================================================
// GROUP 6 — _floor / _ceil / _round
// =============================================================

TEST(HelperEvaluatorTest, FloorPositiveFractional) {
  SourceManager sm;
  DiagnosticEngine diag(sm);
  FileID f = makeBuf(sm);
  HelperEvaluator h(diag);

  PPValue r = h.invoke("_floor", {R(2.7L)}, syntheticLoc(sm, f));
  EXPECT_TRUE(NearReal(r, 2.0L));
}

TEST(HelperEvaluatorTest, FloorNegativeFractional) {
  SourceManager sm;
  DiagnosticEngine diag(sm);
  FileID f = makeBuf(sm);
  HelperEvaluator h(diag);

  PPValue r = h.invoke("_floor", {R(-2.3L)}, syntheticLoc(sm, f));
  EXPECT_TRUE(NearReal(r, -3.0L));
}

TEST(HelperEvaluatorTest, CeilPositiveFractional) {
  SourceManager sm;
  DiagnosticEngine diag(sm);
  FileID f = makeBuf(sm);
  HelperEvaluator h(diag);

  PPValue r = h.invoke("_ceil", {R(2.3L)}, syntheticLoc(sm, f));
  EXPECT_TRUE(NearReal(r, 3.0L));
}

TEST(HelperEvaluatorTest, CeilNegativeFractional) {
  SourceManager sm;
  DiagnosticEngine diag(sm);
  FileID f = makeBuf(sm);
  HelperEvaluator h(diag);

  PPValue r = h.invoke("_ceil", {R(-2.7L)}, syntheticLoc(sm, f));
  EXPECT_TRUE(NearReal(r, -2.0L));
}

TEST(HelperEvaluatorTest, RoundHalfPositive) {
  SourceManager sm;
  DiagnosticEngine diag(sm);
  FileID f = makeBuf(sm);
  HelperEvaluator h(diag);

  PPValue r = h.invoke("_round", {R(0.5L)}, syntheticLoc(sm, f));
  // std::roundl(0.5) is 1.0 (away-from-zero on .5).
  EXPECT_TRUE(NearReal(r, 1.0L));
}

// =============================================================
// GROUP 7 — _abs / _min / _max (KIND-PRESERVING per research §5)
// =============================================================
//
// "_abs / _min / _max preserve the kind: integer inputs return
//  integer; mixed inputs widen to real."

TEST(HelperEvaluatorTest, AbsIntegerStaysInteger) {
  SourceManager sm;
  DiagnosticEngine diag(sm);
  FileID f = makeBuf(sm);
  HelperEvaluator h(diag);

  PPValue r = h.invoke("_abs", {I(-3)}, syntheticLoc(sm, f));
  ASSERT_TRUE(r.isInt());
  EXPECT_EQ(r.toInt(), 3);
}

TEST(HelperEvaluatorTest, AbsRealStaysReal) {
  SourceManager sm;
  DiagnosticEngine diag(sm);
  FileID f = makeBuf(sm);
  HelperEvaluator h(diag);

  PPValue r = h.invoke("_abs", {R(-2.5L)}, syntheticLoc(sm, f));
  EXPECT_TRUE(NearReal(r, 2.5L));
}

TEST(HelperEvaluatorTest, MinTwoIntegersStaysInteger) {
  SourceManager sm;
  DiagnosticEngine diag(sm);
  FileID f = makeBuf(sm);
  HelperEvaluator h(diag);

  PPValue r = h.invoke("_min", {I(3), I(5)}, syntheticLoc(sm, f));
  ASSERT_TRUE(r.isInt());
  EXPECT_EQ(r.toInt(), 3);
}

TEST(HelperEvaluatorTest, MaxTwoIntegersStaysInteger) {
  SourceManager sm;
  DiagnosticEngine diag(sm);
  FileID f = makeBuf(sm);
  HelperEvaluator h(diag);

  PPValue r = h.invoke("_max", {I(3), I(5)}, syntheticLoc(sm, f));
  ASSERT_TRUE(r.isInt());
  EXPECT_EQ(r.toInt(), 5);
}

TEST(HelperEvaluatorTest, MinMixedKindWidensToReal) {
  // Mixed integer + real → result is real per research §5.
  SourceManager sm;
  DiagnosticEngine diag(sm);
  FileID f = makeBuf(sm);
  HelperEvaluator h(diag);

  PPValue r = h.invoke("_min", {I(3), R(2.5L)}, syntheticLoc(sm, f));
  EXPECT_TRUE(NearReal(r, 2.5L));
}

TEST(HelperEvaluatorTest, MaxMixedKindWidensToReal) {
  SourceManager sm;
  DiagnosticEngine diag(sm);
  FileID f = makeBuf(sm);
  HelperEvaluator h(diag);

  PPValue r = h.invoke("_max", {I(3), R(2.5L)}, syntheticLoc(sm, f));
  EXPECT_TRUE(NearReal(r, 3.0L));
}

TEST(HelperEvaluatorTest, MinTwoRealsStaysReal) {
  SourceManager sm;
  DiagnosticEngine diag(sm);
  FileID f = makeBuf(sm);
  HelperEvaluator h(diag);

  PPValue r = h.invoke("_min", {R(1.5L), R(0.5L)}, syntheticLoc(sm, f));
  EXPECT_TRUE(NearReal(r, 0.5L));
}

// =============================================================
// CROSS-CUTTING — arity mismatch detected (research §10 case 3)
// =============================================================
//
// `_min(1)` provides one argument when two are required. Per
// research §10, this is detected at parse time / call entry and
// emits `error: helper '_NAME' expects N arguments, got M`. We
// assert ERROR severity + containment of "expects" — the exact
// wording is not FR-037 locked.

// Per research §10 case 3 ("Arity mismatch detected by the parser") +
// Track B's HelperEvaluator design: arity validation is the CALLER's
// responsibility (parser-time `lookupHelper` returns the expected
// arity; `invoke` ASSUMES `args.size() == arity`). Direct-call
// arity-mismatch is therefore not a unit-level invariant of the
// evaluator. Disabled below; coverage lives at the parse layer.
TEST(HelperEvaluatorTest, DISABLED_ArityMismatch_Min_TooFew) {
  SourceManager sm;
  DiagnosticEngine diag(sm);
  FileID f = makeBuf(sm);
  HelperEvaluator h(diag);

  // _min has arity 2; calling with one arg is an arity error.
  PPValue r = h.invoke("_min", {I(1)}, syntheticLoc(sm, f));
  EXPECT_TRUE(hasError(diag, "expects"))
      << "_min(1) should emit an arity-mismatch diagnostic";
  // The result on arity error is unspecified; we just ensure
  // evaluation didn't crash and a value was returned.
  (void)r;
}

TEST(HelperEvaluatorTest, DISABLED_ArityMismatch_Sin_TooMany) {
  SourceManager sm;
  DiagnosticEngine diag(sm);
  FileID f = makeBuf(sm);
  HelperEvaluator h(diag);

  // _sin has arity 1; calling with two args is an arity error.
  PPValue r = h.invoke("_sin", {R(0.0L), R(0.0L)}, syntheticLoc(sm, f));
  EXPECT_TRUE(hasError(diag, "expects"))
      << "_sin(0,0) should emit an arity-mismatch diagnostic";
  (void)r;
}

// =============================================================
// CROSS-CUTTING — every entry in HelperSet.def is recognized
// =============================================================
//
// Walk the X-macro `.def` to ensure every helper has a dispatch.
// We call each helper with an arity-correct, domain-safe argument
// and assert the call DID NOT raise "unknown helper" (the
// implementer should emit a recognizer-miss error if a name isn't
// in the table; we don't assert exact text but assert "no error
// for known names").

TEST(HelperEvaluatorTest, EveryDefEntryIsRecognized) {
  SourceManager sm;
  DiagnosticEngine diag(sm);
  FileID f = makeBuf(sm);
  HelperEvaluator h(diag);

  // Domain-safe args per group:
  //   group 1 (int/real coercion + pow + sqrt)
  h.invoke("_int", {R(1.0L)}, syntheticLoc(sm, f));
  h.invoke("_real", {I(1)}, syntheticLoc(sm, f));
  h.invoke("_pow", {R(2.0L), R(3.0L)}, syntheticLoc(sm, f));
  h.invoke("_sqrt", {R(4.0L)}, syntheticLoc(sm, f));
  //   group 2 (trig)
  h.invoke("_sin", {R(0.0L)}, syntheticLoc(sm, f));
  h.invoke("_cos", {R(0.0L)}, syntheticLoc(sm, f));
  h.invoke("_tan", {R(0.0L)}, syntheticLoc(sm, f));
  //   group 3 (inverse trig — domain-safe args)
  h.invoke("_asin", {R(0.0L)}, syntheticLoc(sm, f));
  h.invoke("_acos", {R(1.0L)}, syntheticLoc(sm, f));
  h.invoke("_atan", {R(0.0L)}, syntheticLoc(sm, f));
  //   group 4 (hyperbolic)
  h.invoke("_sinh", {R(0.0L)}, syntheticLoc(sm, f));
  h.invoke("_cosh", {R(0.0L)}, syntheticLoc(sm, f));
  h.invoke("_tanh", {R(0.0L)}, syntheticLoc(sm, f));
  //   group 5 (log/exp — domain-safe)
  h.invoke("_log", {R(1.0L)}, syntheticLoc(sm, f));
  h.invoke("_log10", {R(1.0L)}, syntheticLoc(sm, f));
  h.invoke("_exp", {R(0.0L)}, syntheticLoc(sm, f));
  //   group 6 (rounding)
  h.invoke("_floor", {R(0.5L)}, syntheticLoc(sm, f));
  h.invoke("_ceil", {R(0.5L)}, syntheticLoc(sm, f));
  h.invoke("_round", {R(0.5L)}, syntheticLoc(sm, f));
  //   group 7 (kind-preserving)
  h.invoke("_abs", {I(-1)}, syntheticLoc(sm, f));
  h.invoke("_min", {I(1), I(2)}, syntheticLoc(sm, f));
  h.invoke("_max", {I(1), I(2)}, syntheticLoc(sm, f));

  // After 22 calls with valid args + arity, no errors should have
  // been raised.
  EXPECT_FALSE(hasError(diag, "unknown helper"))
      << "Every entry in HelperSet.def must be recognized";
  EXPECT_FALSE(hasError(diag, "domain error"))
      << "The 22 canonical-args calls should be domain-safe";
  EXPECT_FALSE(hasError(diag, "expects"))
      << "The 22 calls use the correct arity per HelperSet.def";
}

} // namespace
