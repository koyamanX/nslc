# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#
# test/lit.cfg.py — root configuration for the nslc lit test suite.
#
# Per research.md §9: one root config; per-layer lit.local.cfg files
# arrive only when a layer needs layer-specific suffixes or
# substitutions. Adding a new fixture is `cp some.test
# test/<Layer>/some.test` — no config edits (FR-007 file-placement
# extensibility).

import os
import shlex
import shutil

import lit.formats

config.name = "nslc"
config.test_format = lit.formats.ShTest(execute_external=True)

# Anything matching a suffix below is discovered as a fixture.
config.suffixes = [".test", ".nsl", ".mlir"]
config.excludes = ["CMakeFiles", "Inputs", "lit.cfg.py", "lit.site.cfg.py"]

# Test-source root and execution-output root come from the site-config
# (configure-time substituted). Locally-developed contributors who run
# lit by hand set them to the repo's `test/` and `build/test/`.
config.test_source_root = os.path.dirname(__file__)
config.test_exec_root = getattr(config, "nslc_obj_root",
                                os.path.join(config.test_source_root, "Output"))

# -----------------------------------------------------------------------------
# Substitutions
# -----------------------------------------------------------------------------
#
# `%nslc`        → ${NSLC_BINARY_DIR}/bin/nslc
# `%FileCheck`   → ${LLVM_TOOLS_BINARY_DIR}/FileCheck
# `%spdx_check`  → ${PYTHON_EXECUTABLE} ${repo}/scripts/check_spdx.py
#
# Each substitution is wired by configure_file via lit.site.cfg.py.in.

llvm_tools = getattr(config, "llvm_tools_binary_dir", "")
nslc_bin   = getattr(config, "nslc_binary_dir", "")
nslc_src   = getattr(config, "nslc_source_dir", "")
python     = getattr(config, "python_executable", "python3")

# M4 dialect fixtures (`test/Dialect/<category>/<op>_roundtrip.mlir`)
# RUN lines invoke `nsl-opt` and `FileCheck` by bare name. Prepend the
# build's `bin/` (where `nsl-opt` and `nslc` live) and the LLVM tools
# directory (where `FileCheck` lives) onto `$PATH` so the bare-name
# invocations resolve.
extra_paths = []
if nslc_bin:
    extra_paths.append(os.path.join(nslc_bin, "bin"))
if llvm_tools:
    extra_paths.append(llvm_tools)
if extra_paths:
    config.environment["PATH"] = os.pathsep.join(
        extra_paths + [config.environment.get("PATH", "")])

# `nsl-opt` and `nslc` are built with -fsanitize=address; the MLIR
# libraries they link against (libMLIR*.so under /opt/llvm) are NOT.
# When the instrumented binary move-assigns an MLIR `SmallVector`
# whose inline storage was poisoned by ASan's container-overflow
# instrumentation but whose receiving end was constructed in a
# non-instrumented translation unit, ASan flags a use-after-poison.
# This is a well-known false positive of partial-ASan instrumentation
# (see LLVM bug #20669 and the upstream MLIR docs); the standard
# workaround is to disable container-overflow checks for the
# child-process environment lit spawns. Other ASan checks (heap,
# stack, use-after-free) remain active.
asan_opts = config.environment.get("ASAN_OPTIONS", "")
extra_asan = "detect_container_overflow=0"
config.environment["ASAN_OPTIONS"] = (
    f"{asan_opts}:{extra_asan}" if asan_opts else extra_asan)

# Substitutions land verbatim inside RUN: shell commands, so paths
# are shell-quoted to survive contributors who clone into a
# whitespace-bearing path or use a Python interpreter at one.
if nslc_bin:
    config.substitutions.append(
        ("%nslc", shlex.quote(os.path.join(nslc_bin, "bin", "nslc"))))
if llvm_tools:
    config.substitutions.append(
        ("%FileCheck", shlex.quote(os.path.join(llvm_tools, "FileCheck"))))
if nslc_src:
    config.substitutions.append(
        ("%spdx_check",
         f"{shlex.quote(python)} "
         f"{shlex.quote(os.path.join(nslc_src, 'scripts', 'check_spdx.py'))}"))

# M7 audited-corpus regression substitutions
# (specs/011-m7-driver-e2e/contracts/audited-corpus.contract.md §5).
#
# `%vcd-diff` → tools/vcd_diff.py invocation (Python stdlib-only
# semantic-equal VCD comparator per Clarifications Q2 → B).
if nslc_src:
    config.substitutions.append(
        ("%vcd-diff",
         f"{shlex.quote(python)} "
         f"{shlex.quote(os.path.join(nslc_src, 'tools', 'vcd_diff.py'))}"))

# Simulator-availability features for the audited-corpus regression.
# Cells under test/audited/<project>_<simulator>.test use
# `REQUIRES: iverilog` or `REQUIRES: verilator` to UNSUPPORTED out
# pre-`:dev-m7` (which is the container that ships the simulators).
# This keeps the cells reviewable as part of the M7 PR without
# turning them RED in CI before the container bumps land.
if shutil.which("iverilog"):
    config.available_features.add("iverilog")
if shutil.which("verilator"):
    config.available_features.add("verilator")

# -----------------------------------------------------------------------------
# Audited-corpus features (T2 T109 — auto-activates on M7 P-VEN vendoring)
# -----------------------------------------------------------------------------
#
# Each Principle-VI-named audited project exposes a lit feature
# `audited-<project>` iff `test/audited/<project>/` exists in the
# source tree. T2's audited idempotence fixtures gate on the
# corresponding feature with `REQUIRES:`, so they auto-activate the
# moment M7 vendors the project tree — no T2-side edit required.
# **Note**: the list below is the *original-pre-narrowing 7-set* —
# the constitution v1.8.0 amendment (2026-05-12) narrowed the M7
# acceptance corpus to 4 (cpu16, mips32_single_cycle, ahb_lite_nsl,
# turboV); the other 3 (`rv32x_dev`, `mmcspi`, `SDRAM_Controler`)
# may re-enter via routine vendoring PRs per the v1.8.0 re-addition
# path. The list here stays at 7 so re-additions auto-feature
# without lit.cfg.py edits.
_audited_projects = [
    "rv32x_dev",
    "turboV",
    "mmcspi",
    "SDRAM_Controler",
    "mips32_single_cycle",
    "ahb_lite_nsl",
    "cpu16",
]
_audited_root = os.path.join(config.test_source_root, "audited")
for _proj in _audited_projects:
    if os.path.isdir(os.path.join(_audited_root, _proj)):
        config.available_features.add(f"audited-{_proj}")
