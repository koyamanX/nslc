// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// lib/Lower/Pass/NSLExpandGeneratePass.cpp — slot 2 of the M5
// structural-expansion pipeline (FR-014).
//
// Implements the unroll: every `nsl.structural_generate` op is
// replaced by N inline copies of its body, where N = (upper - lower
// + step - 1) / step (clamped at 0 if upper <= lower). Each clone's
// `%<loop_var>%` substring inside any `StringAttr` value is replaced
// by the per-iteration integer (decimal). The original op is then
// erased. Nested generates are handled by a fixed-point loop.
//
// Anchors:
//   - `specs/008-m5-structural-passes/spec.md` FR-014, acceptance
//     scenarios 1-7
//   - `specs/008-m5-structural-passes/contracts/pass-pipeline.contract.md`
//     §2 row 2

#include "mlir/IR/Builders.h"
#include "mlir/IR/BuiltinAttributes.h"
#include "mlir/IR/BuiltinOps.h"
#include "mlir/IR/IRMapping.h"
#include "mlir/Pass/Pass.h"
#include "nsl/Dialect/NSL/IR/NSLDialect.h"
#include "nsl/Lower/Lower.h"

#include "llvm/ADT/SmallString.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/StringRef.h"

#include <string>

namespace nsl::lower {

namespace {

/// Substitute every `%<loop_var>%` substring inside `text` with
/// `replacement`. Returns the new string.
///
/// Determinism (Constitution Principle V): pure scan; no map / hash
/// / unordered iteration involved. Linear in the input size.
std::string substituteLoopVar(llvm::StringRef text, llvm::StringRef loop_var,
                              llvm::StringRef replacement) {
  if (loop_var.empty()) {
    return text.str();
  }
  // The pattern we match is literal `%<loop_var>%`. We scan once,
  // appending non-matching prefix + replacement on each hit.
  std::string out;
  out.reserve(text.size());
  size_t pos = 0;
  llvm::SmallString<32> needle;
  needle.push_back('%');
  needle.append(loop_var.begin(), loop_var.end());
  needle.push_back('%');
  llvm::StringRef needleRef = needle.str();
  while (pos < text.size()) {
    auto nextPos = text.find(needleRef, pos);
    if (nextPos == llvm::StringRef::npos) {
      out.append(text.begin() + pos, text.end());
      break;
    }
    out.append(text.begin() + pos, text.begin() + nextPos);
    out.append(replacement.begin(), replacement.end());
    pos = nextPos + needleRef.size();
  }
  return out;
}

/// Walk every `mlir::Attribute` slot on every op inside `block`
/// (recursively) and rewrite any `StringAttr` whose value contains
/// `%<loop_var>%` so the substring is replaced by `valueStr`.
///
/// We only mutate the immediate-attribute layer per the M5 spec
/// FR-018 "MUST NOT walk into nested DictionaryAttr/ArrayAttr" —
/// the same scoping rule applies to substitution to keep behaviour
/// symmetric with residue detection.
void substituteLoopVarInBlock(mlir::Block &block, llvm::StringRef loop_var,
                              llvm::StringRef valueStr) {
  for (mlir::Operation &op : block) {
    llvm::SmallVector<mlir::NamedAttribute, 4> rewrites;
    for (auto namedAttr : op.getAttrs()) {
      auto strAttr = mlir::dyn_cast<mlir::StringAttr>(namedAttr.getValue());
      if (!strAttr) {
        continue;
      }
      auto rewritten =
          substituteLoopVar(strAttr.getValue(), loop_var, valueStr);
      if (rewritten == strAttr.getValue()) {
        continue;
      }
      rewrites.emplace_back(
          namedAttr.getName(),
          mlir::StringAttr::get(strAttr.getContext(), rewritten));
    }
    for (auto &kv : rewrites) {
      op.setAttr(kv.getName(), kv.getValue());
    }
    // Recurse into any regions/blocks the op has — handles a
    // post-clone sub-structure (e.g., a nested
    // `nsl.structural_generate` whose StringAttrs inside its body
    // still need substitution if they contain the OUTER loop_var).
    for (auto &region : op.getRegions()) {
      for (mlir::Block &child : region) {
        substituteLoopVarInBlock(child, loop_var, valueStr);
      }
    }
  }
}

/// Expand a single `nsl.structural_generate` op into N inline
/// copies in its parent block. Returns the number of replicas
/// emitted (0..N).
unsigned expandOne(nsl::dialect::StructuralGenerateOp gen) {
  auto lower_v = static_cast<int64_t>(gen.getLower());
  auto upper_v = static_cast<int64_t>(gen.getUpper());
  auto step_v = static_cast<int64_t>(gen.getStep());
  llvm::StringRef loop_var =
      gen.getLoopVar().has_value() ? *gen.getLoopVar() : llvm::StringRef{};

  // Defensive: dialect verifier rejects step == 0, but we still
  // guard so the pass cannot infinite-loop on malformed input.
  if (step_v == 0) {
    gen.emitOpError() << "step is zero — refusing to expand";
    return 0;
  }
  // Determine direction. Positive step expects lower < upper;
  // reverse expansion (upper < lower with negative step) is also
  // permitted.
  // Iteration count: same convention as Python `range(lower, upper,
  // step)` — exclusive upper, signed-step semantics.
  unsigned count = 0;
  if (step_v > 0) {
    if (upper_v > lower_v) {
      count = static_cast<unsigned>((upper_v - lower_v + step_v - 1) / step_v);
    }
  } else {
    if (lower_v > upper_v) {
      count = static_cast<unsigned>((lower_v - upper_v + (-step_v) - 1) /
                                    (-step_v));
    }
  }

  // Source body block (must exist by SingleBlock trait).
  mlir::Block &srcBlock = gen.getBody().front();

  // Insertion point: just before the structural_generate op so the
  // expanded copies appear in source order at the parent block.
  mlir::OpBuilder builder(gen);

  for (unsigned k = 0; k < count; ++k) {
    int64_t value = lower_v + static_cast<int64_t>(k) * step_v;
    std::string valueStr = std::to_string(value);

    // Clone every op in srcBlock into the parent block at the
    // current insertion point (just before `gen`). IRMapping
    // preserves intra-clone SSA value remapping; cross-region
    // values defined OUTSIDE srcBlock pass through unchanged.
    mlir::IRMapping mapping;
    // Snapshot the insertion-point block — clones land here.
    mlir::Block *insertBlock = builder.getBlock();
    auto insertPoint = builder.getInsertionPoint();
    for (mlir::Operation &op : srcBlock) {
      auto *cloned = op.clone(mapping);
      insertBlock->getOperations().insert(insertPoint, cloned);
    }

    // After cloning, walk the just-inserted ops and rewrite their
    // `StringAttr` values (and recursively those of any sub-ops in
    // their regions) substituting `%<loop_var>%` with `valueStr`.
    // We iterate from the post-clone position back to the original
    // insertion point.
    if (!loop_var.empty() && !srcBlock.empty()) {
      // Identify the cloned range. After insert, the new ops are
      // contiguous in `insertBlock` ending just before `insertPoint`
      // (== `gen`'s iterator). We re-run a per-op visitor over them.
      auto endIt = insertPoint;
      // Walk backward `srcBlock.getOperations().size()` ops to find
      // the start of the cloned range.
      auto count_in_src = std::distance(srcBlock.begin(), srcBlock.end());
      auto startIt = endIt;
      for (size_t i = 0; i < static_cast<size_t>(count_in_src); ++i) {
        --startIt;
      }
      // Visit each cloned op + recurse into its regions.
      for (auto it = startIt; it != endIt; ++it) {
        mlir::Operation &op = *it;
        llvm::SmallVector<mlir::NamedAttribute, 4> rewrites;
        for (auto namedAttr : op.getAttrs()) {
          auto strAttr = mlir::dyn_cast<mlir::StringAttr>(namedAttr.getValue());
          if (!strAttr) {
            continue;
          }
          auto rewritten =
              substituteLoopVar(strAttr.getValue(), loop_var, valueStr);
          if (rewritten == strAttr.getValue()) {
            continue;
          }
          rewrites.emplace_back(
              namedAttr.getName(),
              mlir::StringAttr::get(strAttr.getContext(), rewritten));
        }
        for (auto &kv : rewrites) {
          op.setAttr(kv.getName(), kv.getValue());
        }
        // Recurse into nested regions/blocks for substitution
        // (handles a nested `nsl.structural_generate` body that
        // also references the OUTER loop_var via `%i%`).
        for (auto &region : op.getRegions()) {
          for (mlir::Block &child : region) {
            substituteLoopVarInBlock(child, loop_var, valueStr);
          }
        }
      }
    }
  }

  // Erase the original generate op.
  gen.erase();
  return count;
}

class NSLExpandGeneratePass
    : public mlir::PassWrapper<NSLExpandGeneratePass,
                               mlir::OperationPass<mlir::ModuleOp>> {
public:
  MLIR_DEFINE_EXPLICIT_INTERNAL_INLINE_TYPE_ID(NSLExpandGeneratePass)

  llvm::StringRef getArgument() const final { return "nsl-expand-generate"; }
  llvm::StringRef getDescription() const final {
    return "Slot 2: unroll nsl.structural_generate into N copies of body; "
           "substitute %IDENT% loop-var references (M5 FR-014).";
  }

  void runOnOperation() final {
    mlir::ModuleOp module = getOperation();

    // Fixed-point expansion: repeatedly find any
    // `nsl.structural_generate` op and expand it. Nested generates
    // are handled correctly because expanding an outer generate
    // clones the inner ones VERBATIM into the parent block (the
    // inner's StringAttrs may contain `%<outer-loop-var>%` references
    // which the outer's expansion substitutes in-place); the next
    // iteration then finds and expands the inner clones.
    //
    // Determinism (Constitution Principle V): each iteration uses
    // `walk` which traverses ops in source order; we always expand
    // the first found op, so the output is byte-stable across
    // builds.
    //
    // Termination: every iteration either expands one op (strictly
    // decreasing the count of `StructuralGenerateOp` ops in the
    // module — replicas have NO inner-generate-with-the-same-attrs
    // because cloning preserves nested ops verbatim) or finds none
    // and exits. A pathological input with infinite-recursion
    // generate (not currently expressible at the dialect level)
    // would trigger our defensive iteration cap.
    constexpr unsigned kIterationCap = 1u << 16; // 65536 — safety net
    unsigned iter = 0;
    while (iter++ < kIterationCap) {
      nsl::dialect::StructuralGenerateOp target;
      module.walk([&](nsl::dialect::StructuralGenerateOp gen) {
        target = gen;
        return mlir::WalkResult::interrupt();
      });
      if (!target) {
        break;
      }
      expandOne(target);
    }
    if (iter >= kIterationCap) {
      module.emitOpError() << "expand-generate iteration cap reached ("
                           << kIterationCap
                           << ") — possible non-terminating expansion";
      signalPassFailure();
    }
  }
};

} // namespace

std::unique_ptr<mlir::Pass> createNSLExpandGeneratePass() {
  return std::make_unique<NSLExpandGeneratePass>();
}

} // namespace nsl::lower
