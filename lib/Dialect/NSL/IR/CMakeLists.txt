# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#
# nsl-dialect — layer 7. The `nsl::*` MLIR dialect (TableGen + ODS).
# Per Principle III this layer sits ABOVE stock CIRCT; we never edit
# CIRCT source. M0 skeleton; first ops land at M4.
#
# External link deps (MLIR + CIRCT) come via LINK_LIBS so the
# add_nsl_library dependency-direction guard does not flag them as
# upward intra-NSL deps.

add_nsl_library(nsl-dialect
  LINK_LIBS
    MLIRIR
    CIRCTHWDialect)
