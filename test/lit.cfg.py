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

import lit.formats

config.name = "nslc"
config.test_format = lit.formats.ShTest(execute_external=True)

# Anything matching a suffix below is discovered as a fixture.
config.suffixes = [".test", ".nsl"]
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

if nslc_bin:
    config.substitutions.append(("%nslc", os.path.join(nslc_bin, "bin", "nslc")))
if llvm_tools:
    config.substitutions.append(("%FileCheck", os.path.join(llvm_tools, "FileCheck")))
if nslc_src:
    config.substitutions.append(
        ("%spdx_check",
         f"{python} {os.path.join(nslc_src, 'scripts', 'check_spdx.py')}"))
