# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#
# test/tooling/treesitter/lit.local.cfg.py — disable lit discovery
# under `test/tooling/treesitter/`. Files in this subtree are T8
# tree-sitter test fixtures consumed by `tree-sitter test` (the
# tree-sitter CLI's bundled runner) and `tree-sitter parse` (smoke
# gate), NOT lit RUN-line tests. Without this override, lit would
# pick up the `.nsl` smoke and golden fixtures by suffix and fail
# them with "no RUN line".
#
# This mirrors the precedent set by `test/tooling/lit.local.cfg.py`
# (T1) which disables lit on the broader `test/tooling/` subtree;
# in the present T8 subdirectory the override is reaffirmed at the
# closer scope so adding a new tooling subtree later does not
# accidentally re-enable lit on `treesitter/` via inheritance shifts.
#
# Per the root `test/lit.cfg.py` head comment: "per-layer
# `lit.local.cfg` files arrive only when a layer needs layer-
# specific suffixes or substitutions". This is the `tooling/
# treesitter/` layer.

config.suffixes = []  # noqa: F821 — `config` is injected by lit at load time
