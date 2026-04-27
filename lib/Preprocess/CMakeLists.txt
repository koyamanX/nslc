# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#
# nsl-preprocess — layer 2. NSL preprocessor (#include / #define /
# %IDENT% splicing / #line). M0 skeleton; real implementation at M1.

add_nsl_library(nsl-preprocess
  DEPENDS nsl-basic)
