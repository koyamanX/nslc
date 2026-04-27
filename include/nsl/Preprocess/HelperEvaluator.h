// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// lib/Preprocess/HelperEvaluator.h — PRIVATE header for nsl-preprocess.
//
// Implements data-model entity 13 (`PPValue`) and the helper
// evaluator that backs the closed set of 22 compile-time helpers
// from `include/nsl/Basic/HelperSet.def` (T005, single source of
// truth per FR-017 / research §6).
//
// Numeric model per research §5:
//   - `PPValue` is `std::variant<int64_t, long double>`.
//   - `_int(real)` truncates toward zero; `_real(int)` widens.
//   - Trig/log/exp/sqrt/floor/ceil/round operate on long double.
//   - `_pow` uses `std::powl`.
//   - `_abs`/`_min`/`_max` are KIND-PRESERVING (integer in -> int out;
//     mixed -> real).
//
// Error model per research §10:
//   - Domain error  -> error diagnostic; failsoft return PPValue(0).
//   - Overflow      -> warning; flow IEEE infinity through.
//   - Arity error   -> caught at parse time, never reaches `invoke()`.

#ifndef NSL_LIB_PREPROCESS_HELPEREVALUATOR_H
#define NSL_LIB_PREPROCESS_HELPEREVALUATOR_H

#include "nsl/Basic/Diagnostic.h"
#include "nsl/Basic/SourceLocation.h"

#include <cstdint>
#include <string>
#include <variant>

#include "llvm/ADT/ArrayRef.h"
#include "llvm/ADT/StringRef.h"

namespace nsl::preprocess {

/// Result type of the helper evaluator and `#if` expression evaluator
/// (data-model entity 13). Tagged variant of integer or real.
class PPValue {
public:
  PPValue() noexcept : v_(int64_t{0}) {}
  explicit PPValue(int64_t i) noexcept : v_(i) {}
  explicit PPValue(long double r) noexcept : v_(r) {}

  bool isInt() const noexcept { return std::holds_alternative<int64_t>(v_); }
  bool isReal() const noexcept {
    return std::holds_alternative<long double>(v_);
  }

  /// Return the value as `int64_t`. Reals truncate toward zero
  /// (matches `_int(...)` semantics per research §5).
  int64_t toInt() const;

  /// Return the value as `long double`. Integers widen.
  long double toReal() const;

  /// P4 truth test: non-zero on either kind.
  bool isTruthy() const noexcept;

  /// Render for substitution into the output stream:
  /// integer -> base-10 signed decimal;
  /// real    -> 17-significant-digit decimal (long-double precision
  ///            sufficient to round-trip a `double`). Per research §5.
  std::string render() const;

private:
  std::variant<int64_t, long double> v_;
};

/// Look up a helper name in the closed set defined by
/// `HelperSet.def`. Returns `true` if `name` (with leading `_`) is a
/// recognized helper; `*out_arity` and `*out_returns_real` are set on
/// success. Used by the directive parser to (a) accept helper calls
/// and (b) reject `_<name>(...)` for any `name` not in the set per
/// **P6**.
bool lookupHelper(llvm::StringRef name, int *out_arity, bool *out_returns_real);

/// Evaluate a helper invocation. The arity check is the caller's
/// responsibility (the directive parser detects mismatch at parse
/// time per research §10); `invoke` ASSUMES `args.size() == arity`.
class HelperEvaluator {
public:
  explicit HelperEvaluator(DiagnosticEngine &diag) : diag_(diag) {}

  /// Invoke the helper. `name` includes the leading `_`. `loc` is
  /// the SourceRange of the helper-call site, used for diagnostics
  /// (research §10).
  PPValue invoke(llvm::StringRef name, llvm::ArrayRef<PPValue> args,
                 SourceRange loc);

private:
  DiagnosticEngine &diag_;

  // Per-helper implementation hooks. Each takes already-evaluated
  // arguments; domain errors emit through `diag_` and return `0`.
  PPValue evalIntCoerce(PPValue arg, SourceRange loc);
  PPValue evalRealCoerce(PPValue arg);
  PPValue evalPow(PPValue base, PPValue exp, SourceRange loc);
  PPValue evalSqrt(PPValue arg, SourceRange loc);
  PPValue evalSin(PPValue arg);
  PPValue evalCos(PPValue arg);
  PPValue evalTan(PPValue arg);
  PPValue evalAsin(PPValue arg, SourceRange loc);
  PPValue evalAcos(PPValue arg, SourceRange loc);
  PPValue evalAtan(PPValue arg);
  PPValue evalSinh(PPValue arg);
  PPValue evalCosh(PPValue arg);
  PPValue evalTanh(PPValue arg);
  PPValue evalLog(PPValue arg, SourceRange loc);
  PPValue evalLog10(PPValue arg, SourceRange loc);
  PPValue evalExp(PPValue arg, SourceRange loc);
  PPValue evalFloor(PPValue arg);
  PPValue evalCeil(PPValue arg);
  PPValue evalRound(PPValue arg);
  PPValue evalAbs(PPValue arg);
  PPValue evalMin(PPValue a, PPValue b);
  PPValue evalMax(PPValue a, PPValue b);
};

} // namespace nsl::preprocess

#endif // NSL_LIB_PREPROCESS_HELPEREVALUATOR_H
