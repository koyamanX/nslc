// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// lib/Preprocess/HelperEvaluator.cpp — closed-set helper evaluator
// (T055). Numeric model per research §5; error model per research
// §10. The dispatch table is built from `HelperSet.def` (T005, single
// source of truth) so adding a 23rd helper is a one-line spec patch
// (FR-017).

#include "nsl/Preprocess/HelperEvaluator.h"

#include "nsl/Basic/Diagnostic.h"
#include "nsl/Basic/SourceLocation.h"

#include "llvm/ADT/ArrayRef.h"
#include "llvm/ADT/StringRef.h"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <limits>
#include <string>
#include <variant>

namespace nsl::preprocess {

// -----------------------------------------------------------------------------
// PPValue
// -----------------------------------------------------------------------------

int64_t PPValue::toInt() const {
  if (std::holds_alternative<int64_t>(v_)) {
    return std::get<int64_t>(v_);
  }
  long double r = std::get<long double>(v_);
  // Truncate toward zero per `_int(...)` semantics (research §5).
  // Guard against NaN / infinity which would otherwise produce
  // implementation-defined results on the cast.
  if (std::isnan(r) || std::isinf(r)) {
    return 0;
  }
  long double t = std::trunc(r);
  if (t > static_cast<long double>(std::numeric_limits<int64_t>::max())) {
    return std::numeric_limits<int64_t>::max();
  }
  if (t < static_cast<long double>(std::numeric_limits<int64_t>::min())) {
    return std::numeric_limits<int64_t>::min();
  }
  return static_cast<int64_t>(t);
}

long double PPValue::toReal() const {
  if (std::holds_alternative<long double>(v_)) {
    return std::get<long double>(v_);
  }
  return static_cast<long double>(std::get<int64_t>(v_));
}

bool PPValue::isTruthy() const noexcept {
  if (isInt()) {
    return std::get<int64_t>(v_) != 0;
  }
  long double r = std::get<long double>(v_);
  return r != static_cast<long double>(0.0);
}

std::string PPValue::render() const {
  if (isInt()) {
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%lld",
                  static_cast<long long>(std::get<int64_t>(v_)));
    return std::string(buf);
  }
  // Real: 17 significant digits, sufficient to round-trip a double
  // (research §5). Use `%.17Lg` for long-double-aware formatting.
  char buf[64];
  std::snprintf(buf, sizeof(buf), "%.17Lg", std::get<long double>(v_));
  // If the rendered text contains no `.`, no `e`, and no `E`, it is
  // an integer-valued real. Append `.0` so the downstream lexer sees
  // it as a float literal (which the preprocessor immediately
  // detects at the seam check; this keeps P7 enforcement honest).
  std::string s(buf);
  bool has_decimal = false;
  for (char c : s) {
    if (c == '.' || c == 'e' || c == 'E' || c == 'n' /* nan */ ||
        c == 'i' /* inf */) {
      has_decimal = true;
      break;
    }
  }
  if (!has_decimal) {
    s += ".0";
  }
  return s;
}

// -----------------------------------------------------------------------------
// Helper recognizer
// -----------------------------------------------------------------------------

namespace {

struct HelperEntry {
  const char *name; // includes leading underscore
  int arity;
  bool returns_real;
};

constexpr HelperEntry kHelpers[] = {
#define HELPER(NAME, ARITY, RETURNS_REAL)                                      \
  HelperEntry{"_" #NAME, ARITY, RETURNS_REAL},
#include "nsl/Basic/HelperSet.def"
#undef HELPER
};

constexpr std::size_t kHelperCount = sizeof(kHelpers) / sizeof(kHelpers[0]);

} // namespace

bool lookupHelper(llvm::StringRef name, int *out_arity,
                  bool *out_returns_real) {
  for (std::size_t i = 0; i < kHelperCount; ++i) {
    if (name == llvm::StringRef(kHelpers[i].name)) {
      if (out_arity != nullptr) {
        *out_arity = kHelpers[i].arity;
      }
      if (out_returns_real != nullptr) {
        *out_returns_real = kHelpers[i].returns_real;
      }
      return true;
    }
  }
  return false;
}

// -----------------------------------------------------------------------------
// Per-helper evaluation
// -----------------------------------------------------------------------------

namespace {

bool isOverflowReal(long double r) {
  return std::isinf(r) || std::isnan(r);
}

} // namespace

PPValue HelperEvaluator::evalIntCoerce(PPValue arg, SourceRange /*loc*/) {
  return PPValue(arg.toInt());
}

PPValue HelperEvaluator::evalRealCoerce(PPValue arg) {
  return PPValue(arg.toReal());
}

PPValue HelperEvaluator::evalPow(PPValue base, PPValue exp, SourceRange loc) {
  long double b = base.toReal();
  long double e = exp.toReal();
  long double r = ::powl(b, e);
  if (isOverflowReal(r)) {
    diag_.report(Severity::Warning, loc.begin(),
                 "helper '_pow' result exceeds long-double range");
  }
  return PPValue(r);
}

PPValue HelperEvaluator::evalSqrt(PPValue arg, SourceRange loc) {
  long double v = arg.toReal();
  if (v < 0) {
    diag_.report(Severity::Error, loc.begin(),
                 "helper '_sqrt' domain error: argument must be non-negative");
    return PPValue(int64_t{0});
  }
  return PPValue(::sqrtl(v));
}

PPValue HelperEvaluator::evalSin(PPValue arg) {
  return PPValue(::sinl(arg.toReal()));
}
PPValue HelperEvaluator::evalCos(PPValue arg) {
  return PPValue(::cosl(arg.toReal()));
}
PPValue HelperEvaluator::evalTan(PPValue arg) {
  return PPValue(::tanl(arg.toReal()));
}

PPValue HelperEvaluator::evalAsin(PPValue arg, SourceRange loc) {
  long double v = arg.toReal();
  if (v < -1.0L || v > 1.0L) {
    diag_.report(Severity::Error, loc.begin(),
                 "helper '_asin' domain error: argument must be in [-1, 1]");
    return PPValue(int64_t{0});
  }
  return PPValue(::asinl(v));
}

PPValue HelperEvaluator::evalAcos(PPValue arg, SourceRange loc) {
  long double v = arg.toReal();
  if (v < -1.0L || v > 1.0L) {
    diag_.report(Severity::Error, loc.begin(),
                 "helper '_acos' domain error: argument must be in [-1, 1]");
    return PPValue(int64_t{0});
  }
  return PPValue(::acosl(v));
}

PPValue HelperEvaluator::evalAtan(PPValue arg) {
  return PPValue(::atanl(arg.toReal()));
}

PPValue HelperEvaluator::evalSinh(PPValue arg) {
  return PPValue(::sinhl(arg.toReal()));
}
PPValue HelperEvaluator::evalCosh(PPValue arg) {
  return PPValue(::coshl(arg.toReal()));
}
PPValue HelperEvaluator::evalTanh(PPValue arg) {
  return PPValue(::tanhl(arg.toReal()));
}

PPValue HelperEvaluator::evalLog(PPValue arg, SourceRange loc) {
  long double v = arg.toReal();
  if (v <= 0.0L) {
    diag_.report(Severity::Error, loc.begin(),
                 "helper '_log' domain error: argument must be positive");
    return PPValue(int64_t{0});
  }
  return PPValue(::logl(v));
}

PPValue HelperEvaluator::evalLog10(PPValue arg, SourceRange loc) {
  long double v = arg.toReal();
  if (v <= 0.0L) {
    diag_.report(Severity::Error, loc.begin(),
                 "helper '_log10' domain error: argument must be positive");
    return PPValue(int64_t{0});
  }
  return PPValue(::log10l(v));
}

PPValue HelperEvaluator::evalExp(PPValue arg, SourceRange loc) {
  long double r = ::expl(arg.toReal());
  if (isOverflowReal(r)) {
    diag_.report(Severity::Warning, loc.begin(),
                 "helper '_exp' result exceeds long-double range");
  }
  return PPValue(r);
}

PPValue HelperEvaluator::evalFloor(PPValue arg) {
  return PPValue(::floorl(arg.toReal()));
}
PPValue HelperEvaluator::evalCeil(PPValue arg) {
  return PPValue(::ceill(arg.toReal()));
}
PPValue HelperEvaluator::evalRound(PPValue arg) {
  return PPValue(::roundl(arg.toReal()));
}

PPValue HelperEvaluator::evalAbs(PPValue arg) {
  // KIND-PRESERVING: integer in -> integer out; real in -> real out.
  if (arg.isInt()) {
    int64_t v = arg.toInt();
    // Guard INT64_MIN: |INT64_MIN| is not representable.
    if (v == std::numeric_limits<int64_t>::min()) {
      return PPValue(std::numeric_limits<int64_t>::max());
    }
    return PPValue(v < 0 ? -v : v);
  }
  long double v = arg.toReal();
  return PPValue(::fabsl(v));
}

PPValue HelperEvaluator::evalMin(PPValue a, PPValue b) {
  // Mixed kinds widen to real. Same kind preserves kind.
  if (a.isInt() && b.isInt()) {
    int64_t x = a.toInt();
    int64_t y = b.toInt();
    return PPValue(x < y ? x : y);
  }
  long double x = a.toReal();
  long double y = b.toReal();
  return PPValue(x < y ? x : y);
}

PPValue HelperEvaluator::evalMax(PPValue a, PPValue b) {
  if (a.isInt() && b.isInt()) {
    int64_t x = a.toInt();
    int64_t y = b.toInt();
    return PPValue(x > y ? x : y);
  }
  long double x = a.toReal();
  long double y = b.toReal();
  return PPValue(x > y ? x : y);
}

// -----------------------------------------------------------------------------
// Dispatch
// -----------------------------------------------------------------------------

PPValue HelperEvaluator::invoke(llvm::StringRef name,
                                llvm::ArrayRef<PPValue> args, SourceRange loc) {
  // Single-arg helpers.
  auto unary = [&](PPValue (HelperEvaluator::*fn)(PPValue)) {
    return (this->*fn)(args[0]);
  };
  auto unary_loc = [&](PPValue (HelperEvaluator::*fn)(PPValue, SourceRange)) {
    return (this->*fn)(args[0], loc);
  };

  if (name == "_int") {
    return evalIntCoerce(args[0], loc);
  }
  if (name == "_real") {
    return evalRealCoerce(args[0]);
  }
  if (name == "_pow") {
    return evalPow(args[0], args[1], loc);
  }
  if (name == "_sqrt") {
    return unary_loc(&HelperEvaluator::evalSqrt);
  }
  if (name == "_sin") {
    return unary(&HelperEvaluator::evalSin);
  }
  if (name == "_cos") {
    return unary(&HelperEvaluator::evalCos);
  }
  if (name == "_tan") {
    return unary(&HelperEvaluator::evalTan);
  }
  if (name == "_asin") {
    return unary_loc(&HelperEvaluator::evalAsin);
  }
  if (name == "_acos") {
    return unary_loc(&HelperEvaluator::evalAcos);
  }
  if (name == "_atan") {
    return unary(&HelperEvaluator::evalAtan);
  }
  if (name == "_sinh") {
    return unary(&HelperEvaluator::evalSinh);
  }
  if (name == "_cosh") {
    return unary(&HelperEvaluator::evalCosh);
  }
  if (name == "_tanh") {
    return unary(&HelperEvaluator::evalTanh);
  }
  if (name == "_log") {
    return unary_loc(&HelperEvaluator::evalLog);
  }
  if (name == "_log10") {
    return unary_loc(&HelperEvaluator::evalLog10);
  }
  if (name == "_exp") {
    return unary_loc(&HelperEvaluator::evalExp);
  }
  if (name == "_floor") {
    return unary(&HelperEvaluator::evalFloor);
  }
  if (name == "_ceil") {
    return unary(&HelperEvaluator::evalCeil);
  }
  if (name == "_round") {
    return unary(&HelperEvaluator::evalRound);
  }
  if (name == "_abs") {
    return unary(&HelperEvaluator::evalAbs);
  }
  if (name == "_min") {
    return evalMin(args[0], args[1]);
  }
  if (name == "_max") {
    return evalMax(args[0], args[1]);
  }
  // Unknown helper — should have been rejected at parse time. Return
  // safe default.
  diag_.report(Severity::Error, loc.begin(),
               ("unknown compile-time helper: '" + name.str() + "'"));
  return PPValue(int64_t{0});
}

} // namespace nsl::preprocess
