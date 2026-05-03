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

#include "mlir/IR/BuiltinAttributes.h"
#include "mlir/IR/BuiltinOps.h"
#include "mlir/Pass/Pass.h"
#include "nsl/Dialect/NSL/IR/NSLDialect.h"
#include "nsl/Lower/Lower.h"

#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/StringMap.h"
#include "llvm/ADT/StringRef.h"

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
    module.walk(
        [&](mlir::Operation *op) { diagCount += scanOpAttrsForResidue(op); });

    // Step 2 — sensitive-Sn re-checks per `pass-pipeline.contract.md`
    // §3. Three of the six rows have meaningful structural-only
    // re-checks at M5 (S10, S16, S25); the other three (S6, S15,
    // S20) are documented stubs — see comments below + the XFAIL'd
    // fixtures `test/Lower/passes/nsl-check-semantics/s{6,15,20}_*.mlir`.
    diagCount += checkS10LoopVarResidue(module);
    diagCount += checkS16PureNsl(module);
    diagCount += checkS25ReplicatedCollision(module);

    // **S6 — use-before-def** (DEFERRED at M5): requires SSA
    // operand-traversal across regions. MLIR's SSA verifier already
    // catches the most pathological forms; a meaningful re-check
    // needs cross-replica name-tracking + canonical-version
    // resolution which exceeds the slot-6 budget. Helper body lands
    // when an M5+ amendment adds the operand-traversal infra.
    //
    // **S15 — bit-slice non-const** (VACUOUS at M5): `nsl.slice`
    // carries `I64Attr` lo/hi indices, not operand-side SSA values.
    // The "non-constant after slot 1" condition is unreachable on
    // pure-NSL inputs. Helper body lands when an op variant whose
    // index is operand-side `Value` is added.
    //
    // **S20 — submod-array iface** (POST-M6): interface-modifier
    // bindings are an M6 surface; the structural shape that would
    // trigger this re-check is unrepresentable at M5. Helper body
    // lands when M6 adds `nsl.submod_iface_bind` (or the eventual
    // op).

    if (diagCount > 0) {
      signalPassFailure();
    }
  }

private:
  /// **S10 re-check**: a `nsl.structural_generate` op surviving to
  /// slot 6 means slot 2 (expand-generate) was skipped or buggy.
  /// FROZEN diagnostic per §3 row S10.
  unsigned checkS10LoopVarResidue(mlir::ModuleOp module) {
    unsigned count = 0;
    module.walk([&](nsl::dialect::StructuralGenerateOp gen) {
      llvm::StringRef loopVar =
          gen.getLoopVar().has_value() ? *gen.getLoopVar() : llvm::StringRef{};
      gen.emitError() << "'generate' loop variable '%" << loopVar
                      << "%' not eliminated by structural expansion";
      ++count;
    });
    return count;
  }

  /// **S16 re-check**: a `nsl.param_int` / `nsl.param_str` op
  /// surviving in a pure-NSL module is meaningful only for V/V/SC
  /// submodules per S16. At M5 ALL submodules are pure-NSL, so any
  /// surviving param op qualifies. FROZEN diagnostic per §3 row S16.
  ///
  /// (When M7 introduces V/V/SC submodule lowering, this helper
  /// MUST be amended to walk the submodule list and skip the diag
  /// if any submodule's template resolves to a non-NSL kind.)
  unsigned checkS16PureNsl(mlir::ModuleOp module) {
    unsigned count = 0;
    // Source-order walk via `getOps<>()` — deterministic.
    for (auto p : module.getOps<nsl::dialect::ParamIntOp>()) {
      p.emitError() << "parameter '@" << p.getSymName()
                    << "' meaningful only for V/V/SC submodules";
      ++count;
    }
    for (auto p : module.getOps<nsl::dialect::ParamStrOp>()) {
      p.emitError() << "parameter '@" << p.getSymName()
                    << "' meaningful only for V/V/SC submodules";
      ++count;
    }
    return count;
  }

  /// **S25 re-check**: two declaration-bearing ops with the same
  /// `name` `StringAttr` value in the same scope. Post-expand-
  /// generate, an unrolled body's per-iteration name-substitution
  /// (`%i%` → `0`/`1`/...) MUST uniquify each replica; if a body
  /// declares an unsubstituted name (e.g., `reg buf_const`), the
  /// replicas collide.
  ///
  /// Scope: the immediate `nsl.module` body (the only scope the
  /// post-pipeline shape produces collisions in at M5). Decl ops
  /// considered: `nsl.reg`, `nsl.wire`, `nsl.variable`, `nsl.mem`
  /// (each carries a `name` `StringAttr`). FROZEN diagnostic per
  /// §3 row S25.
  unsigned checkS25ReplicatedCollision(mlir::ModuleOp module) {
    unsigned count = 0;
    module.walk([&](nsl::dialect::ModuleOp nslMod) {
      // Track names seen in source-order in this module's body.
      // `StringMap` lookup is by-key (StringRef), insertion order
      // doesn't affect emission since we only emit on the SECOND
      // hit and the walk is source-ordered.
      llvm::StringMap<mlir::Operation *> seen;
      for (mlir::Operation &op : nslMod.getBody().front()) {
        // Each declaration-bearing storage op carries a `name`
        // StringAttr at attribute slot "name" (`nsl.reg`,
        // `nsl.wire`, `nsl.variable`, `nsl.mem`).
        if (!mlir::isa<nsl::dialect::RegOp, nsl::dialect::WireOp,
                       nsl::dialect::VariableOp, nsl::dialect::MemOp>(op)) {
          continue;
        }
        auto nameAttr = op.getAttrOfType<mlir::StringAttr>("name");
        if (!nameAttr) {
          continue;
        }
        auto [it, inserted] = seen.try_emplace(nameAttr.getValue(), &op);
        if (!inserted) {
          // Second occurrence — emit diag on the COLLIDING op (not
          // the first).
          op.emitError() << "duplicate declaration '" << nameAttr.getValue()
                         << "' in replicated 'generate' body";
          ++count;
        }
      }
    });
    return count;
  }
};

} // namespace

std::unique_ptr<mlir::Pass> createNSLCheckSemanticsPass() {
  return std::make_unique<NSLCheckSemanticsPass>();
}

} // namespace nsl::lower
