// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// lib/Lower/Pass/NSLCheckSemanticsPass.cpp — slot 6 of the M5
// structural-expansion pipeline (FR-018).
//
// Two responsibilities, both running in a single
// `runOnOperation()` invocation per
// `pass-pipeline.contract.md` §4 (multi-error within one pass):
//
//   (1) **Residue detection** — regex scan over every reachable
//       `mlir::StringAttr` for unresolved `%IDENT%` splice tokens
//       (`residue-detection.contract.md` §1, §2). Each match emits
//       `error: unresolved macro splice '%<IDENT>%' after structural expansion`
//       (frozen by FR-018 + `residue-detection.contract.md` §4).
//
//   (2) **Sensitive-Sn re-checks** — the six post-expansion-sensitive
//       `Sn` constraints (S6/S10/S15/S16/S20/S25 per
//       `pass-pipeline.contract.md` §3). Diagnostic strings frozen
//       per Principle VIII; helper bodies land at T097.
//
// Diagnostic strings are FROZEN per Constitution Principle VIII —
// renaming is a contract amendment that updates `pass-pipeline.contract.md`
// §3 + every fixture in the same patch.
//
// Anchors:
//   - `specs/008-m5-structural-passes/spec.md` FR-018
//   - `specs/008-m5-structural-passes/contracts/residue-detection.contract.md`
//   - `specs/008-m5-structural-passes/contracts/pass-pipeline.contract.md`
//     §3, §4

#include "nsl/Lower/Lower.h"

#include "nsl/Dialect/NSL/IR/NSLDialect.h"

#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/StringRef.h"
#include "mlir/IR/BuiltinAttributes.h"
#include "mlir/IR/BuiltinOps.h"
#include "mlir/Pass/Pass.h"

#include <regex>
#include <string>

namespace nsl::lower {

namespace {

/// FROZEN regex per `residue-detection.contract.md` §2:
///
///     R"((%[A-Za-z_][A-Za-z0-9_]*%))"
///
/// `%` literal start, identifier body per pp.ebnf §5, `%` literal
/// end. ECMAScript-flavour (the `std::regex` default) — character
/// classes used here are portable across that flavour and POSIX.
/// Function-local-static so initialisation is one-time per process
/// and thread-safe (C++11 magic statics).
const std::regex &residueRegex() {
  static const std::regex kRe(R"((%[A-Za-z_][A-Za-z0-9_]*%))");
  return kRe;
}

/// Run the residue regex over `text` and emit one diagnostic per
/// non-overlapping match against `op`. Returns the number of
/// matches found (== diagnostics emitted).
unsigned scanStringForResidue(mlir::Operation *op, llvm::StringRef text) {
  // `std::regex_iterator` requires `std::string::const_iterator` or
  // `const char*`. We use the `cregex_iterator` over the raw range
  // [begin, end) — no std::string copy needed.
  const char *begin = text.data();
  const char *end = begin + text.size();
  std::cregex_iterator it(begin, end, residueRegex());
  std::cregex_iterator stop;
  unsigned count = 0;
  for (; it != stop; ++it) {
    auto match = it->str();
    // `match` includes the surrounding `%` characters; strip them
    // for the diagnostic. `%[A-Za-z_][A-Za-z0-9_]*%` always has at
    // least 3 chars (e.g., `%X%`) so this is safe.
    auto innerStart = match.size() >= 2 ? 1u : 0u;
    auto innerLen = match.size() >= 2 ? match.size() - 2 : 0u;
    std::string ident = match.substr(innerStart, innerLen);
    op->emitError() << "unresolved macro splice '%" << ident
                    << "%' after structural expansion";
    ++count;
  }
  return count;
}

/// Walk every named attribute on `op` and scan every `StringAttr`
/// value for residue. Symbol references (`FlatSymbolRefAttr`) are
/// ALSO scanned per `residue-detection.contract.md` §3 (the contract
/// note that `FlatSymbolRefAttr::getValue()` is a `StringRef` so the
/// same regex applies). `IntegerAttr` / `TypeAttr` / nested
/// `DictionaryAttr` / `ArrayAttr` are NOT scanned — explicit
/// non-recursion per FR-018 last sentence.
unsigned scanOpAttrsForResidue(mlir::Operation *op) {
  unsigned total = 0;
  for (auto namedAttr : op->getAttrs()) {
    auto attrValue = namedAttr.getValue();
    if (auto strAttr = mlir::dyn_cast<mlir::StringAttr>(attrValue)) {
      total += scanStringForResidue(op, strAttr.getValue());
    } else if (auto symAttr =
                   mlir::dyn_cast<mlir::FlatSymbolRefAttr>(attrValue)) {
      total += scanStringForResidue(op, symAttr.getValue());
    }
    // Nested attribute kinds intentionally not recursed.
  }
  return total;
}

class NSLCheckSemanticsPass
    : public mlir::PassWrapper<NSLCheckSemanticsPass,
                               mlir::OperationPass<mlir::ModuleOp>> {
public:
  MLIR_DEFINE_EXPLICIT_INTERNAL_INLINE_TYPE_ID(NSLCheckSemanticsPass)

  llvm::StringRef getArgument() const final { return "nsl-check-semantics"; }
  llvm::StringRef getDescription() const final {
    return "Slot 6: regex-detect %IDENT% residue across nsl::* StringAttr "
           "values + re-check the six post-expansion-sensitive Sn "
           "(S6/S10/S15/S16/S20/S25) (M5 FR-018).";
  }

  void runOnOperation() final {
    mlir::ModuleOp module = getOperation();

    // Multi-error: per `pass-pipeline.contract.md` §4, we MUST emit
    // ALL diagnostics for ALL violations in a single
    // `runOnOperation()` invocation. The pass calls
    // `signalPassFailure()` IFF the running diagnostic count is > 0.
    unsigned diagCount = 0;

    // Step 1 — residue detection. Walk in source order
    // (Constitution Principle V — determinism). Every op
    // (including the top-level module) has its named-attribute
    // dictionary scanned.
    module.walk([&](mlir::Operation *op) {
      diagCount += scanOpAttrsForResidue(op);
    });

    // Step 2 — sensitive-Sn re-checks. Helper-stub for T097.
    // (No-op at Commit 2; lands at Commit 3 for S15/S16/S25.)

    if (diagCount > 0) {
      signalPassFailure();
    }
  }
};

} // namespace

std::unique_ptr<mlir::Pass> createNSLCheckSemanticsPass() {
  return std::make_unique<NSLCheckSemanticsPass>();
}

} // namespace nsl::lower
