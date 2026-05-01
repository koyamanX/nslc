// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// lib/Dialect/NSL/IR/NSLOps.cpp — dialect op-class glue and
// hand-written verifier bodies for `nsl-dialect` (M4, layer 7).
//
// **Specification anchors**:
//   - `specs/007-m4-mlir-dialect/spec.md` FR-009–FR-013 — full op
//     set + structural-invariant verifier table (Q1 Option A:
//     structural-only; Q2 Option B: hand-walk for transitive parents).
//   - `specs/007-m4-mlir-dialect/data-model.md` §2 — per-op trait
//     set and verifier-implementation style.
//   - `specs/007-m4-mlir-dialect/data-model.md` §4 — verifier-helper
//     utilities. NOTE: `findAncestorOfKind<T>` (proposed in
//     research.md §4 / data-model §4 / T098) is dropped per F9
//     carry-over — it's redundant with upstream
//     `op->getParentOfType<T>()`. Only `emitParentMismatch` and
//     `isRegLikeValue` are defined here.
//   - `specs/007-m4-mlir-dialect/research.md` §4 — verifier-impl
//     language (hand-written C++ in this file).
//
// Phase 4 (US2, T100–T118) fills the bodies with structural-invariant
// checks. Trait-only ops (per data-model §2 "Verifier style") rely on
// MLIR's TableGen-emitted trait verifiers and keep `success()` stubs
// or no `verify()` declaration at all.

#include "nsl/Dialect/NSL/IR/NSLDialect.h"

#include "mlir/IR/SymbolTable.h"
#include "llvm/ADT/SmallPtrSet.h"

// Op-class definitions (constructors / accessors / parser / printer
// emitted by TableGen via `GET_OP_CLASSES`) MOVED to
// `NSLDialect.cpp` so `addOperations<>()` has them complete in the
// dialect's `initialize()` body. This file keeps only the hand-
// written `verify()` bodies (per data-model §2 "Verifier style"
// column) plus the verifier-helper utilities (data-model §4).

namespace nsl::dialect {

namespace {

// ---------------------------------------------------------------------------
// Verifier helpers (data-model §4 — F9 carry-over: `findAncestorOfKind`
// dropped in favor of upstream `op->getParentOfType<T>()`).
// ---------------------------------------------------------------------------

/// Emit the standard "must be enclosed by 'nsl.<kind>'" diagnostic for a
/// transitive-parent miss (Q2 Option B). Returns `failure()` so callers
/// can `return emitParentMismatch(op, "seq");` directly.
mlir::LogicalResult emitParentMismatch(mlir::Operation *op,
                                       llvm::StringRef expectedKind) {
  return op->emitOpError()
         << "must be enclosed by 'nsl." << expectedKind << "'";
}

/// True iff `v` is the result of an `nsl.reg` op or an `nsl.field` op
/// extracting from a reg-typed source (per data-model §4 helper).
/// Used by `nsl.clocked_transfer` and `nsl.incdec` to validate the
/// destination operand is a register-like SSA value.
bool isRegLikeValue(mlir::Value v) {
  mlir::Operation *defining = v.getDefiningOp();
  if (!defining) {
    return false;
  }
  if (mlir::isa<RegOp>(defining)) {
    return true;
  }
  if (auto field = mlir::dyn_cast<FieldOp>(defining)) {
    // Recurse: a field-of-reg-struct (or a field-of-(field-of-reg))
    // counts as reg-like. The recursion terminates at a non-Field /
    // non-Reg defining op (returns false).
    return isRegLikeValue(field.getSource());
  }
  return false;
}

/// Compute the total bit-width of an `nsl.struct<@T>` by resolving the
/// symbol to the declaring `nsl.struct` op and summing the widths of
/// its `nsl.field_decl` children. Returns `std::nullopt` if any field
/// type is non-bits (e.g., a nested struct, which the per-cell
/// invariant for `nsl.struct_cast` width-match doesn't cover at M4)
/// or if symbol resolution fails.
std::optional<unsigned> computeStructTotalWidth(mlir::Operation *user,
                                                StructType structTy) {
  // Resolve the struct symbol against the nearest enclosing op that
  // implements SymbolTable (the parent `nsl.module`).
  mlir::Operation *nearestSymTable =
      mlir::SymbolTable::getNearestSymbolTable(user);
  if (!nearestSymTable) {
    return std::nullopt;
  }
  mlir::Operation *target = mlir::SymbolTable::lookupSymbolIn(
      nearestSymTable, structTy.getName());
  auto structDecl = mlir::dyn_cast_or_null<StructOp>(target);
  if (!structDecl) {
    return std::nullopt;
  }
  unsigned total = 0;
  for (mlir::Operation &child : structDecl.getBody().front()) {
    auto fdecl = mlir::dyn_cast<FieldDeclOp>(&child);
    if (!fdecl) {
      // Unexpected non-field-decl child — defer to the future
      // amendment that supports nested struct fields.
      return std::nullopt;
    }
    auto bitsTy = mlir::dyn_cast<BitsType>(fdecl.getFieldType());
    if (!bitsTy) {
      // Nested struct fields are out of scope for the M4 width
      // check (per FR-013 row for `nsl.struct_cast`).
      return std::nullopt;
    }
    total += bitsTy.getWidth();
  }
  return total;
}

/// Look up the `nsl.field_decl` op at index `idx` inside the struct
/// declaration that `structTy` symbolically refers to. Returns the
/// op (or nullptr if unresolved / out-of-range).
FieldDeclOp lookupFieldDeclByIndex(mlir::Operation *user,
                                   StructType structTy,
                                   uint64_t idx) {
  mlir::Operation *nearestSymTable =
      mlir::SymbolTable::getNearestSymbolTable(user);
  if (!nearestSymTable) {
    return {};
  }
  mlir::Operation *target = mlir::SymbolTable::lookupSymbolIn(
      nearestSymTable, structTy.getName());
  auto structDecl = mlir::dyn_cast_or_null<StructOp>(target);
  if (!structDecl) {
    return {};
  }
  uint64_t i = 0;
  for (mlir::Operation &child : structDecl.getBody().front()) {
    auto fdecl = mlir::dyn_cast<FieldDeclOp>(&child);
    if (!fdecl) {
      continue;
    }
    if (i == idx) {
      return fdecl;
    }
    ++i;
  }
  return {};
}

} // namespace

// ===========================================================================
// 2.1 Module-level verifiers
// ===========================================================================

mlir::LogicalResult ModuleOp::verify() {
  // `Symbol` trait machinery already enforces `sym_name` presence, but
  // the generic-form fixture `module_invalid_no_symname.mlir`
  // constructs the op without ANY `sym_name` attribute, which the
  // upstream Symbol-trait verifier rejects with a diagnostic
  // containing "sym_name". We add a defensive check here so the
  // diagnostic substring is stable even if MLIR's wording shifts.
  auto symAttr = (*this)->getAttrOfType<mlir::StringAttr>(
      mlir::SymbolTable::getSymbolAttrName());
  if (!symAttr || symAttr.getValue().empty()) {
    return emitOpError() << "requires a non-empty 'sym_name' attribute";
  }
  return mlir::success();
}

mlir::LogicalResult StructOp::verify() {
  // `sym_name` presence (defensive — see ModuleOp::verify rationale).
  auto symAttr = (*this)->getAttrOfType<mlir::StringAttr>(
      mlir::SymbolTable::getSymbolAttrName());
  if (!symAttr || symAttr.getValue().empty()) {
    return emitOpError() << "requires a non-empty 'sym_name' attribute";
  }
  // Field-list non-circular: walk every `nsl.field_decl` child whose
  // type is `!nsl.struct<@T>`; refuse if `@T` resolves transitively
  // back to *this* struct.
  llvm::SmallPtrSet<mlir::Operation *, 8> seen;
  std::function<bool(StructOp)> reachesSelf =
      [&](StructOp current) -> bool {
    if (current == *this) {
      return true;
    }
    if (!seen.insert(current.getOperation()).second) {
      return false;
    }
    for (mlir::Operation &child : current.getBody().front()) {
      auto fdecl = mlir::dyn_cast<FieldDeclOp>(&child);
      if (!fdecl) {
        continue;
      }
      auto structTy = mlir::dyn_cast<StructType>(fdecl.getFieldType());
      if (!structTy) {
        continue;
      }
      // Resolve the referenced struct symbolically.
      mlir::Operation *nearestSymTable =
          mlir::SymbolTable::getNearestSymbolTable(current);
      if (!nearestSymTable) {
        continue;
      }
      auto referenced = mlir::dyn_cast_or_null<StructOp>(
          mlir::SymbolTable::lookupSymbolIn(nearestSymTable,
                                            structTy.getName()));
      if (!referenced) {
        continue;
      }
      if (reachesSelf(referenced)) {
        return true;
      }
    }
    return false;
  };
  for (mlir::Operation &child : getBody().front()) {
    auto fdecl = mlir::dyn_cast<FieldDeclOp>(&child);
    if (!fdecl) {
      continue;
    }
    auto structTy = mlir::dyn_cast<StructType>(fdecl.getFieldType());
    if (!structTy) {
      continue;
    }
    mlir::Operation *nearestSymTable =
        mlir::SymbolTable::getNearestSymbolTable(*this);
    if (!nearestSymTable) {
      continue;
    }
    auto referenced = mlir::dyn_cast_or_null<StructOp>(
        mlir::SymbolTable::lookupSymbolIn(nearestSymTable,
                                          structTy.getName()));
    if (!referenced) {
      continue;
    }
    seen.clear();
    if (reachesSelf(referenced)) {
      return emitOpError() << "struct field list is circular through field '"
                           << fdecl.getName() << "'";
    }
  }
  return mlir::success();
}

mlir::LogicalResult ConnectOp::verify() {
  // Q3 Option A — strict `mlir::Type` equality on operands. The
  // `SameTypeOperands` trait already enforces this in the printable
  // form, but the generic-form fixture
  // `connect_invalid_type_mismatch.mlir` constructs the op via
  // `"nsl.connect"(...)` — exercising the type-mismatch path.
  if (getDst().getType() != getSrc().getType()) {
    return emitOpError() << "operand type mismatch: dst has "
                         << getDst().getType() << ", src has "
                         << getSrc().getType();
  }
  return mlir::success();
}

// ===========================================================================
// 2.2 Storage verifiers
// ===========================================================================

mlir::LogicalResult RegOp::verify() {
  mlir::Type t = getResult().getType();
  if (!mlir::isa<BitsType>(t) && !mlir::isa<StructType>(t)) {
    return emitOpError() << "result type must be '!nsl.bits<N>' or "
                            "'!nsl.struct<@T>', got "
                         << t;
  }
  return mlir::success();
}

mlir::LogicalResult WireOp::verify() {
  if (!mlir::isa<BitsType>(getResult().getType())) {
    return emitOpError() << "result type must be '!nsl.bits<N>', got "
                         << getResult().getType();
  }
  return mlir::success();
}

mlir::LogicalResult VariableOp::verify() {
  if (!mlir::isa<BitsType>(getResult().getType())) {
    return emitOpError() << "result type must be '!nsl.bits<N>', got "
                         << getResult().getType();
  }
  return mlir::success();
}

mlir::LogicalResult MemOp::verify() {
  if (!mlir::isa<MemType>(getResult().getType())) {
    return emitOpError() << "result type must be '!nsl.mem<[D x T]>', got "
                         << getResult().getType();
  }
  return mlir::success();
}

// ===========================================================================
// 2.2bis Constant verifier (post-merge M4-amendment 2026-05-01)
// ===========================================================================

mlir::LogicalResult ConstantOp::verify() {
  // Per the post-merge amendment: `value` (an I64) must fit in the result
  // bits-type's width. We use unsigned-domain comparison — the I64Attr is
  // semantically the bit-pattern of an N-bit unsigned integer, so a value
  // of `(1 << N) - 1` is the largest legal pattern at width `N`.
  auto bitsTy = mlir::cast<BitsType>(getResult().getType());
  unsigned width = bitsTy.getWidth();
  uint64_t raw = static_cast<uint64_t>(getValue());
  if (width == 0) {
    if (raw != 0) {
      return emitOpError() << "value " << raw
                           << " does not fit in '!nsl.bits<0>' "
                              "(zero-width type admits only the value 0)";
    }
    return mlir::success();
  }
  if (width >= 64) {
    // At width == 64, every I64 bit-pattern is admissible. Widths >64
    // are deferred to a future amendment (the value attr is I64Attr).
    return mlir::success();
  }
  uint64_t mask = (uint64_t{1} << width) - 1;
  if (raw & ~mask) {
    return emitOpError() << "value " << raw
                         << " does not fit in '!nsl.bits<" << width << ">'";
  }
  return mlir::success();
}

// ===========================================================================
// 2.4 Action-block verifiers
// ===========================================================================

mlir::LogicalResult AltOp::verify() {
  // ≥ 1 child whose kind is `nsl.case` or `nsl.default`.
  for (mlir::Operation &child : getBody().front()) {
    if (mlir::isa<CaseOp, DefaultOp>(child)) {
      return mlir::success();
    }
  }
  return emitOpError() << "requires at least one case-or-default child";
}

mlir::LogicalResult AnyOp::verify() {
  for (mlir::Operation &child : getBody().front()) {
    if (mlir::isa<CaseOp, DefaultOp>(child)) {
      return mlir::success();
    }
  }
  return emitOpError() << "requires at least one case-or-default child";
}

mlir::LogicalResult WhileOp::verify() {
  if (!(*this)->getParentOfType<SeqOp>()) {
    return emitParentMismatch(*this, "seq");
  }
  return mlir::success();
}

mlir::LogicalResult ForOp::verify() {
  if (!(*this)->getParentOfType<SeqOp>()) {
    return emitParentMismatch(*this, "seq");
  }
  // Loop-bound-attr shape: if a `loop_bound` attribute is present, it
  // MUST be an integer attribute (per the FR-013 row for `nsl.for`).
  if (auto attr = (*this)->getAttr("loop_bound")) {
    if (!mlir::isa<mlir::IntegerAttr>(attr)) {
      return emitOpError()
             << "'loop_bound' attribute must be an integer (loop-bound "
                "attribute shape)";
    }
  }
  return mlir::success();
}

// ===========================================================================
// 2.6 Atomic verifiers
// ===========================================================================

mlir::LogicalResult ClockedTransferOp::verify() {
  if (!isRegLikeValue(getDst())) {
    return emitOpError() << "destination operand must be reg-like (an "
                            "'nsl.reg' result or an 'nsl.field' of a "
                            "reg-typed struct)";
  }
  return mlir::success();
}

mlir::LogicalResult IncDecOp::verify() {
  if (!isRegLikeValue(getDst())) {
    return emitOpError()
           << "destination operand must be reg-like (an 'nsl.reg' result "
              "or an 'nsl.field' of a reg-typed struct)";
  }
  // The `IncDecKindAttr` enum-attr machinery rejects out-of-range
  // values during attribute parsing; for generic-form constructions
  // that bypass the dialect-attr parser (e.g., `kind = 99 : i32`),
  // verify the raw attr is a valid `IncDecKind` enum.
  auto kindAttr = (*this)->getAttr("kind");
  if (!kindAttr) {
    return emitOpError() << "missing 'kind' enum attribute";
  }
  // If TableGen's typed `IncDecKindAttr` decoded successfully, the
  // typed accessor returns a value in the enum range. If decoding
  // failed (raw I32Attr survives), the typed accessor would not be
  // reachable via the generic-form constructor — check the raw form.
  if (auto i32 = mlir::dyn_cast<mlir::IntegerAttr>(kindAttr)) {
    auto raw = i32.getValue().getZExtValue();
    if (raw > static_cast<uint64_t>(IncDecKind::PostDec)) {
      return emitOpError() << "'kind' attribute value " << raw
                           << " is out of range for 'IncDecKind' enum";
    }
  }
  return mlir::success();
}

mlir::LogicalResult CallOp::verify() {
  // The control-terminal sibling ops (`nsl.func_in`, `nsl.func_out`,
  // `nsl.func_self`) carry their identifying string in a `StrAttr`
  // named `name` (NOT in `sym_name`), so the standard SymbolTable
  // resolver doesn't match them. Walk the enclosing `nsl.module`
  // body and match on the `name` StrAttr.
  auto moduleOp = (*this)->getParentOfType<ModuleOp>();
  if (!moduleOp) {
    return mlir::success();
  }
  llvm::StringRef callee = getCallee();
  unsigned argCount = static_cast<unsigned>(getArgs().size());
  for (mlir::Operation &child : moduleOp.getBody().front()) {
    if (auto fin = mlir::dyn_cast<FuncInOp>(&child)) {
      if (fin.getName() == callee) {
        if (fin.getArgs().size() != argCount) {
          return emitOpError() << "arg count mismatch: callee '" << callee
                               << "' expects " << fin.getArgs().size()
                               << ", got " << argCount;
        }
        return mlir::success();
      }
    } else if (auto fout = mlir::dyn_cast<FuncOutOp>(&child)) {
      if (fout.getName() == callee) {
        if (fout.getArgs().size() != argCount) {
          return emitOpError() << "arg count mismatch: callee '" << callee
                               << "' expects " << fout.getArgs().size()
                               << ", got " << argCount;
        }
        return mlir::success();
      }
    } else if (auto fself = mlir::dyn_cast<FuncSelfOp>(&child)) {
      if (fself.getName() == callee) {
        if (fself.getArgs().size() != argCount) {
          return emitOpError() << "arg count mismatch: callee '" << callee
                               << "' expects " << fself.getArgs().size()
                               << ", got " << argCount;
        }
        return mlir::success();
      }
    }
  }
  // Unresolved callee — defer to upstream SymbolTable's diagnostic
  // (M4 is conservative here per Q1 Option A; structural-only).
  return mlir::success();
}

mlir::LogicalResult FinishOp::verify() {
  if (!(*this)->getParentOfType<ProcOp>()) {
    return emitParentMismatch(*this, "proc");
  }
  return mlir::success();
}

// ===========================================================================
// 2.7 Procedure verifiers
// ===========================================================================

mlir::LogicalResult ProcOp::verify() {
  // `sym_name` presence (defensive — see ModuleOp::verify rationale).
  auto symAttr = (*this)->getAttrOfType<mlir::StringAttr>(
      mlir::SymbolTable::getSymbolAttrName());
  if (!symAttr || symAttr.getValue().empty()) {
    return emitOpError() << "requires a non-empty 'sym_name' attribute";
  }
  // At most one `nsl.first_state` child.
  unsigned firstStateCount = 0;
  for (mlir::Operation &child : getBody().front()) {
    if (mlir::isa<FirstStateOp>(child)) {
      ++firstStateCount;
    }
  }
  if (firstStateCount > 1) {
    return emitOpError()
           << "must contain at most one 'nsl.first_state' child, got "
           << firstStateCount;
  }
  return mlir::success();
}

mlir::LogicalResult FirstStateOp::verify() {
  // The `target` symbol ref SHOULD resolve to a sibling `nsl.state`
  // op. Per Q1 Option A (structural-only), if the symbol is unresolved
  // OR resolves to something other than `nsl.state`, this MAY be a
  // structural error. However, M4 is conservative here: we only
  // diagnose when the symbol resolves to a *non-state* sibling
  // (definitive structural mismatch); unresolved symrefs defer to
  // upstream's SymbolTable diagnostic and allow surrounding
  // verifiers (e.g. the parent `nsl.state`'s `sym_name` check) to
  // surface first.
  auto procOp = (*this)->getParentOfType<ProcOp>();
  if (!procOp) {
    return mlir::success();
  }
  mlir::Operation *resolved = mlir::SymbolTable::lookupSymbolIn(
      procOp, getTargetAttr());
  if (resolved && !mlir::isa<StateOp>(resolved)) {
    return emitOpError() << "'target' symbol ref '" << getTarget()
                         << "' resolves to a non-'nsl.state' sibling";
  }
  // The fail-case fixture `first_state_invalid_no_target.mlir`
  // expects substring "target" — emit a diagnostic when no sibling
  // `nsl.state` whose `sym_name` matches the target is found.
  // CAUTION: a sibling `nsl.state` may itself be malformed (missing
  // `sym_name`) — read the attribute defensively so this verifier
  // never crashes on a malformed sibling (the malformed sibling's
  // own `Symbol`-trait verifier surfaces the diagnostic).
  bool foundState = false;
  bool sawMalformedState = false;
  for (mlir::Operation &child : procOp.getBody().front()) {
    auto state = mlir::dyn_cast<StateOp>(&child);
    if (!state) {
      continue;
    }
    auto siblingSym = state->getAttrOfType<mlir::StringAttr>(
        mlir::SymbolTable::getSymbolAttrName());
    if (!siblingSym) {
      sawMalformedState = true;
      continue;
    }
    if (siblingSym.getValue() == getTarget()) {
      foundState = true;
      break;
    }
  }
  if (!foundState && !sawMalformedState) {
    return emitOpError() << "'target' '" << getTarget()
                         << "' does not name a sibling 'nsl.state'";
  }
  return mlir::success();
}

// ===========================================================================
// 2.8 Procedure-helper verifier
// ===========================================================================

mlir::LogicalResult GotoOp::verify() {
  // Q2 Option B two-kind transitive parent: `nsl.seq` (label form) OR
  // `nsl.state` (state-name form). We synthesize a combined-substring
  // diagnostic on miss so both forms are surfaced.
  auto seqAncestor = (*this)->getParentOfType<SeqOp>();
  auto stateAncestor = (*this)->getParentOfType<StateOp>();
  if (!seqAncestor && !stateAncestor) {
    return emitOpError()
           << "must be enclosed by 'nsl.seq' (label form) or 'nsl.state' "
              "(state-name form)";
  }
  // Resolve `target`. State-name form: enclosing `nsl.proc` symbol
  // table. Label form: not yet implemented in this dialect (M4 has no
  // label op), so fall through to the state-form check.
  if (auto procOp = (*this)->getParentOfType<ProcOp>()) {
    mlir::Operation *resolved = mlir::SymbolTable::lookupSymbolIn(
        procOp, getTargetAttr());
    if (!mlir::isa_and_nonnull<StateOp>(resolved)) {
      return emitOpError() << "'target' symbol ref '" << getTarget()
                           << "' does not resolve to a sibling 'nsl.state'";
    }
  }
  return mlir::success();
}

// ===========================================================================
// 2.10 Marker verifiers
// ===========================================================================

mlir::LogicalResult FireProbeOp::verify() {
  // The `target` symbol ref MUST name a sibling `nsl.func_in` /
  // `nsl.func_out` / `nsl.func_self`. Note: those control-terminal
  // ops do NOT carry the `Symbol` trait at M4 (their identifying
  // string lives in a `StrAttr` named `name`, not in `sym_name`).
  // So we resolve manually: walk the enclosing `nsl.module`'s body
  // and match on the `name` StrAttr of any sibling control terminal.
  auto moduleOp = (*this)->getParentOfType<ModuleOp>();
  if (!moduleOp) {
    return mlir::success();
  }
  llvm::StringRef target = getTarget();
  for (mlir::Operation &child : moduleOp.getBody().front()) {
    if (auto fin = mlir::dyn_cast<FuncInOp>(&child)) {
      if (fin.getName() == target) {
        return mlir::success();
      }
    } else if (auto fout = mlir::dyn_cast<FuncOutOp>(&child)) {
      if (fout.getName() == target) {
        return mlir::success();
      }
    } else if (auto fself = mlir::dyn_cast<FuncSelfOp>(&child)) {
      if (fself.getName() == target) {
        return mlir::success();
      }
    }
  }
  return emitOpError() << "'target' '" << target
                       << "' does not name a sibling 'nsl.func_in', "
                          "'nsl.func_out', or 'nsl.func_self'";
}

mlir::LogicalResult StructCastOp::verify() {
  // Width-match: the bits side's width must equal the struct side's
  // total width (per FR-013 row for `nsl.struct_cast`).
  mlir::Type srcTy = getSource().getType();
  mlir::Type dstTy = getResult().getType();
  unsigned bitsWidth = 0;
  std::optional<unsigned> structWidth;
  if (auto srcBits = mlir::dyn_cast<BitsType>(srcTy)) {
    bitsWidth = srcBits.getWidth();
    if (auto dstStruct = mlir::dyn_cast<StructType>(dstTy)) {
      structWidth = computeStructTotalWidth(*this, dstStruct);
    } else if (mlir::isa<BitsType>(dstTy)) {
      // bits→bits is a vacuous cast; require width-equal.
      if (bitsWidth != mlir::cast<BitsType>(dstTy).getWidth()) {
        return emitOpError() << "bits→bits cast width mismatch: source "
                             << bitsWidth << ", result "
                             << mlir::cast<BitsType>(dstTy).getWidth();
      }
      return mlir::success();
    }
  } else if (auto srcStruct = mlir::dyn_cast<StructType>(srcTy)) {
    structWidth = computeStructTotalWidth(*this, srcStruct);
    if (auto dstBits = mlir::dyn_cast<BitsType>(dstTy)) {
      bitsWidth = dstBits.getWidth();
    } else {
      // struct→struct: defer to the future amendment (M4 only covers
      // bits↔struct casts per FR-013).
      return mlir::success();
    }
  }
  if (structWidth && *structWidth != bitsWidth) {
    return emitOpError() << "struct_cast width mismatch: bits side "
                         << bitsWidth << ", struct side " << *structWidth;
  }
  return mlir::success();
}

mlir::LogicalResult FieldOp::verify() {
  auto structTy = mlir::cast<StructType>(getSource().getType());
  uint64_t idx = getIndex();
  // Range-check: index ∈ [0, numFields).
  // We resolve the struct decl and count its `nsl.field_decl` children.
  mlir::Operation *nearestSymTable =
      mlir::SymbolTable::getNearestSymbolTable(*this);
  if (!nearestSymTable) {
    return mlir::success();
  }
  auto structDecl = mlir::dyn_cast_or_null<StructOp>(
      mlir::SymbolTable::lookupSymbolIn(nearestSymTable,
                                        structTy.getName()));
  if (!structDecl) {
    return mlir::success();
  }
  uint64_t numFields = 0;
  for (mlir::Operation &child : structDecl.getBody().front()) {
    if (mlir::isa<FieldDeclOp>(child)) {
      ++numFields;
    }
  }
  if (idx >= numFields) {
    return emitOpError() << "field 'index' " << idx
                         << " is out of range [0, " << numFields << ")";
  }
  // Result-type-match: the result's type must equal the field decl's
  // declared type at that index.
  if (auto fdecl = lookupFieldDeclByIndex(*this, structTy, idx)) {
    if (fdecl.getFieldType() != getResult().getType()) {
      return emitOpError() << "result type " << getResult().getType()
                           << " does not match struct field's declared "
                              "type "
                           << fdecl.getFieldType();
    }
  }
  return mlir::success();
}

// ===========================================================================
// 2.11 Expansion-only verifier
// ===========================================================================

mlir::LogicalResult StructuralGenerateOp::verify() {
  // Loop-bound-attr shape: `lower`, `upper`, `step` must be present
  // (TableGen-trait enforces presence + integer kind); additionally
  // `step` MUST be non-zero so the expansion bounds describe a
  // terminating loop (per FR-013 row for `nsl.structural_generate`).
  if (getStep() == 0) {
    return emitOpError() << "'step' attribute must be non-zero "
                            "(loop-bound shape requires a "
                            "terminating expansion)";
  }
  return mlir::success();
}

} // namespace nsl::dialect
