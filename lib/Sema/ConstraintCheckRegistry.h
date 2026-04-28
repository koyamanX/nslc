// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// lib/Sema/ConstraintCheckRegistry.h - private impl header for the
// per-Sn checker registry. NOT a public header; lives under
// lib/Sema/ per the sema-api.contract.md Invariant 1 freeze on
// include/nsl/Sema/.
//
// Each per-Sn source under lib/Sema/Constraints/S<NN>_*.cpp
// declares a private subclass of ConstraintVisitor and registers
// it at static-init time via the NSL_REGISTER_CONSTRAINT(N, T)
// macro. Sema::runConstraintPasses iterates the registry in
// Sn-numeric order (deterministic per Principle V) and invokes
// run() on each visitor.
//
// Pattern is LLVM INITIALIZE_PASS / libtooling check registries.

#ifndef NSL_SEMA_CONSTRAINT_CHECK_REGISTRY_H
#define NSL_SEMA_CONSTRAINT_CHECK_REGISTRY_H

#include <cstdint>
#include <memory>

namespace nsl {
class DiagnosticEngine;
} // namespace nsl

namespace nsl::ast {
class CompilationUnit;
} // namespace nsl::ast

namespace nsl::sema {

class SymbolTable;
class TypeSystem;
struct ResolutionMap;

/// Aggregates the post-resolution context every per-Sn checker
/// reads. Passed by const-ref to ConstraintVisitor::run() so
/// individual checkers cannot accidentally mutate it.
struct ConstraintContext {
  const ast::CompilationUnit *unit;
  SymbolTable *symbols;
  TypeSystem *types;
  const ResolutionMap *resolutions;
  DiagnosticEngine *diag;
};

/// Abstract base for every per-Sn checker. Each S<NN>_*.cpp ships
/// a private subclass implementing run() and self-registers at
/// static-init time.
class ConstraintVisitor {
public:
  ConstraintVisitor() = default;
  virtual ~ConstraintVisitor();
  ConstraintVisitor(const ConstraintVisitor &) = delete;
  ConstraintVisitor &operator=(const ConstraintVisitor &) = delete;
  ConstraintVisitor(ConstraintVisitor &&) = delete;
  ConstraintVisitor &operator=(ConstraintVisitor &&) = delete;

  /// Walk ctx.unit and emit any S<NN> violations to ctx.diag.
  /// MUST be deterministic (Principle V) and idempotent.
  virtual void run(const ConstraintContext &ctx) const = 0;
};

/// Register visitor to fire on S<sn>. Lower sn runs first
/// (Sn-numeric order; Principle V determinism). Called at
/// static-init time by NSL_REGISTER_CONSTRAINT.
void registerConstraint(unsigned sn,
                        std::unique_ptr<ConstraintVisitor> visitor);

/// Run every registered constraint visitor in Sn-numeric order
/// against ctx. Called by Sema::runConstraintPasses.
void runAllConstraints(const ConstraintContext &ctx);

/// Self-registration helper. Each S<NN>_*.cpp writes:
///   namespace { struct S<NN>Visitor : ConstraintVisitor { ... }; }
///   NSL_REGISTER_CONSTRAINT(<NN>, S<NN>Visitor)
/// at file scope. The macro expands to a static initializer that
/// constructs a single instance and inserts it into the registry.
#define NSL_REGISTER_CONSTRAINT(SN, VisitorClass)                              \
  namespace {                                                                  \
  struct VisitorClass##Registrar {                                             \
    VisitorClass##Registrar() {                                                \
      ::nsl::sema::registerConstraint(                                         \
          (SN), std::make_unique<VisitorClass>());                             \
    }                                                                          \
  };                                                                           \
  static VisitorClass##Registrar g_##VisitorClass##_registrar;                 \
  } // namespace

} // namespace nsl::sema

#endif // NSL_SEMA_CONSTRAINT_CHECK_REGISTRY_H
