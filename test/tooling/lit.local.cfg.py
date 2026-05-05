# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#
# test/tooling/lit.local.cfg.py — disable lit discovery under
# `test/tooling/`. Files in this subtree are tooling-test fixtures
# consumed by their own runner (e.g. vscode-tmgrammar-test for the
# T1 TextMate scope tests under `test/tooling/textmate/`), not lit
# RUN-line tests. Without this override, lit picks up the `.nsl`
# fixtures by suffix and fails them with "no RUN line".
#
# Per the root `test/lit.cfg.py` head comment: "per-layer
# `lit.local.cfg` files arrive only when a layer needs layer-
# specific suffixes or substitutions". This is the `tooling/` layer.

config.suffixes = []
