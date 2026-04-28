// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// lib/Sema/ConstraintCheckRegistry.cpp - per-Sn checker registry.
// Each S<NN>_*.cpp self-registers via NSL_REGISTER_CONSTRAINT at
// static-init time. The fan-out happens in Sn-numeric order at the
// end of Sema::run() (orchestrated from Sema.cpp).

#include "ConstraintCheckRegistry.h"

#include <map>
#include <memory>
#include <utility>
#include <vector>

namespace nsl::sema {

ConstraintVisitor::~ConstraintVisitor() = default;

namespace {

// File-static registry keyed by Sn-number. std::map is iteration-
// order-deterministic (sorted by key), which is exactly what we
// want: lower Sn runs first per Principle V.
std::map<unsigned, std::vector<std::unique_ptr<ConstraintVisitor>>> &
registry() {
  static std::map<unsigned, std::vector<std::unique_ptr<ConstraintVisitor>>>
      r;
  return r;
}

} // namespace

void registerConstraint(unsigned sn,
                        std::unique_ptr<ConstraintVisitor> visitor) {
  if (!visitor) {
    return;
  }
  registry()[sn].push_back(std::move(visitor));
}

void runAllConstraints(const ConstraintContext &ctx) {
  for (const auto &kv : registry()) {
    for (const auto &v : kv.second) {
      v->run(ctx);
    }
  }
}

} // namespace nsl::sema
