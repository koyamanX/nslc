# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#
# test/lsp/lit.local.cfg.py — disable lit's suffix-based discovery
# for this subtree. The LSP integration tests are gtest binaries
# driven by ctest, NOT lit. Without this opt-out, lit's recursive
# discovery would find the `.nsl` fixture files and fail them with
# "no RUN line" (the fixtures are pure NSL source, consumed via
# `LspSession::sendNotification('textDocument/didOpen', ...)`,
# not via FileCheck).
#
# Mirrors the precedent set by `test/tooling/lit.local.cfg.py`
# (T1 / TextMate scope tests).

config.suffixes = []
config.excludes = ["fixtures", "CMakeLists.txt", "lit.local.cfg.py"]
