// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// lib/Driver/Sema.cpp — thin driver-side wrapper around
// `nsl::sema::Sema::run`. ~30 lines per `tasks.md` T016 sizing.
//
// Used by `lib/Driver/EmitAST.cpp` (and forward `-emit=*` paths) to
// invoke Sema after a successful parse without re-publishing the
// `Sema` engine ctor at every consumer site.

#include "nsl/Driver/Sema.h"

#include "nsl/AST/CompilationUnit.h"
#include "nsl/Basic/Diagnostic.h"
#include "nsl/Sema/Sema.h"

namespace nsl::driver {

sema::SemaResult runSema(ast::CompilationUnit &unit,
                         DiagnosticEngine     &diag) {
  sema::Sema sema(diag);
  return sema.run(unit);
}

} // namespace nsl::driver
