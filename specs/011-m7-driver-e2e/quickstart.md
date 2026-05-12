<!-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception -->

# Quickstart: M7 — `nsl-driver` end-to-end + P-VEN + P-VCD + audited-corpus regression

**Branch**: `011-m7-driver-e2e`
**Date**: 2026-05-11
**Audience**: contributors implementing or reviewing M7

This quickstart shows how to build, run, and test M7's deliverables
end-to-end inside the dev container. Every command runs inside
the M7 dev container (`ghcr.io/koyamanX/nsl-nslc:dev-m7`); host
machines do NOT need Verilator / Icarus / Python 3.11 / riscv-tests.

> **Reminder**: per the project memory `project_build_environment.md`,
> all `cmake` / `ninja` / `lit` / `gtest` runs MUST happen inside
> the dev container. Outside-container build attempts will fail
> because the host has no LLVM/MLIR install.

---

## 1. Get into the M7 dev container

```sh
# From the host:
sg docker -c 'docker pull ghcr.io/koyamanX/nsl-nslc:dev-m7'
sg docker -c 'docker run --rm -it \
    -v $PWD:/work -w /work \
    ghcr.io/koyamanX/nsl-nslc:dev-m7 bash'
```

(If you used `:dev` previously, that's fine for non-M7 work, but
the audited-corpus regression below requires `:dev-m7`.)

Inside the container:

```sh
# Verify the M7 tools are present.
verilator --version    # ⇒ Verilator 5.024 ...
iverilog -V            # ⇒ Icarus Verilog version 11.x
riscv32-unknown-elf-gcc --version   # ⇒ riscv32-unknown-elf-gcc 13.2.0
python3 --version      # ⇒ Python 3.11.x
```

---

## 2. Configure + build `nslc`

```sh
cmake -G Ninja -B build-noasan \
    -DCMAKE_BUILD_TYPE=Release \
    -DMLIR_DIR=/opt/llvm/lib/cmake/mlir \
    -DCIRCT_DIR=/opt/circt/lib/cmake/circt \
    -DNSL_ENABLE_ASAN=OFF \
    -S .
cmake --build build-noasan -j --target nslc
```

Build the M7-new TUs along the way (they're part of `nsl-driver`,
which `nslc` links against):

```sh
ls build-noasan/lib/Driver/
# Should include CMakeFiles/nsl-driver.dir/
#                EmitVerilog.cpp.o
#                RunCIRCTPasses.cpp.o
```

---

## 3. Smoke-test `nslc -emit=verilog` on a representative source

A minimal source under `test/Driver/emit_verilog/fixtures/single_module.nsl`:

```nsl
declare m {
  output q[8];
}

module m {
  reg r[8] = 0;

  func_self {
    r := r + 1;
    q = r;
  }
}
```

Compile to Verilog:

```sh
# Stdout mode (single-file):
./build-noasan/bin/nslc -emit=verilog test/Driver/emit_verilog/fixtures/single_module.nsl
# Expected: a single-module Verilog body printed to stdout,
# with one always_ff @(posedge m_clock or posedge p_reset) and
# one assign for the output.

# File mode (single-file written to disk):
./build-noasan/bin/nslc -emit=verilog \
    test/Driver/emit_verilog/fixtures/single_module.nsl \
    -o /tmp/m.v

# Split-file mode (per-module .v files in a directory):
mkdir -p /tmp/m-verilog
./build-noasan/bin/nslc -emit=verilog \
    test/Driver/emit_verilog/fixtures/single_module.nsl \
    -o /tmp/m-verilog/
ls /tmp/m-verilog/
# ⇒ m.v  (one file per hw.module — for a single-module source,
#         one file produced)
```

Run the determinism check:

```sh
./build-noasan/bin/nslc -emit=verilog \
    test/Driver/emit_verilog/fixtures/single_module.nsl \
    -o /tmp/run1.v
./build-noasan/bin/nslc -emit=verilog \
    test/Driver/emit_verilog/fixtures/single_module.nsl \
    -o /tmp/run2.v
diff -q /tmp/run1.v /tmp/run2.v
# ⇒ (no output; exit 0)
```

Run the simulator smoke checks:

```sh
iverilog -g2012 -o /tmp/m.vvp /tmp/m.v
# Expected: no syntax errors.

verilator --lint-only /tmp/m.v
# Expected: no syntax errors (warnings allowed but not errors).
```

---

## 4. Run the driver-unit lit fixtures

```sh
cmake --build build-noasan --target check-emit-verilog
# Or via lit directly:
./build-noasan/bin/llvm-lit -v test/Driver/emit_verilog/
```

Expected: all 10 fixtures from `driver-emit-verilog.contract.md` §7 pass.

---

## 5. Run the audited-corpus regression

Prerequisite: P-VEN must be done (the seven projects vendored
under `test/audited/`).

```sh
cmake --build build-noasan --target check-audited
```

Behavior: lit enumerates per-cell test instances, runs `nslc -emit=verilog`
per project, compiles + simulates under both iverilog and verilator,
captures VCDs, runs `tools/vcd_diff.py` to compare against golden
VCDs.

Wall-clock target: ≤ 15 minutes total.

Per-cell logs land under `build-noasan/test/audited/<project>/<simulator>/<scenario>.log`
for inspection (FR-022).

Expected output on a passing run:

```text
PASS: test/audited/cpu16/cpu16_basic.iverilog.test  (0.8s)
PASS: test/audited/cpu16/cpu16_basic.verilator.test (1.2s)
PASS: test/audited/mips32_single_cycle/mips_hello.iverilog.test (1.5s)
PASS: test/audited/mips32_single_cycle/mips_hello.verilator.test (2.0s)
PASS: test/audited/ahb_lite_nsl/ahb_read.iverilog.test (0.9s)
PASS: test/audited/ahb_lite_nsl/ahb_read.verilator.test (1.1s)
...
PASS: test/audited/rv32x_dev/add.iverilog.test (2.5s)
PASS: test/audited/rv32x_dev/add.verilator.test (3.8s)
PASS: test/audited/rv32x_dev/load.iverilog.test (2.7s)
PASS: test/audited/rv32x_dev/load.verilator.test (3.9s)
...
PASS: test/audited/turboV/branch.iverilog.test (3.0s)
PASS: test/audited/turboV/branch.verilator.test (4.1s)

Testing Time: ~12m45s
  Expected Passes : 28
  Expected Failures : 0
  Unexpected Failures : 0
```

A red cell looks like:

```text
FAIL: test/audited/cpu16/cpu16_basic.verilator.test (1.4s)
******************** TEST 'cpu16_basic.verilator' FAILED ********************
$ "tools/vcd_diff.py" "test/audited/cpu16/golden/cpu16_basic.vcd" \
    "build-noasan/test/audited/cpu16/verilator/cpu16_basic.vcd"

VCD divergence:
  Golden:  test/audited/cpu16/golden/cpu16_basic.vcd
  Emitted: build-noasan/test/audited/cpu16/verilator/cpu16_basic.vcd

First divergence at simulation time #150:
  Signal:  top.cpu.regfile.r1[15:0]
  Golden value:  16'h0042
  Emitted value: 16'h0041

Context (last 3 value changes on this signal):
  #100 golden=0040 emitted=0040
  #120 golden=0041 emitted=0041
  #140 golden=0042 emitted=0041  ← divergence

Total signals matched: 47
Signals unmatched on golden side: 0
Signals unmatched on emitted side: 2
```

The error message + per-cell `.log` file give a complete picture
for debugging.

---

## 6. Run the manual VCD-diff (debugging)

When debugging a regression manually:

```sh
python3 tools/vcd_diff.py -v \
    test/audited/cpu16/golden/cpu16_basic.vcd \
    /tmp/my-debugging-vcd.vcd
```

The `-v` (verbose) flag adds INFO lines for unmatched signals so
you can see "what's in golden but not in emitted" and vice-versa.

---

## 7. Add a new audited project (post-M7, routine)

Per SC-006 + audited-corpus contract §8, adding an 8th project
is infra-free:

```sh
# 1. Vendor the upstream sources.
mkdir -p test/audited/<new-project>/{tb,golden}
cp -r /path/to/upstream/<new-project>/* test/audited/<new-project>/

# 2. Author PROVENANCE.md (use any existing project as a template).

# 3. Author golden/REGEN.md and run the recipe to generate goldens.

# 4. (Optional) Author golden/SIGNAL_MAP.toml if signal names differ.

# 5. Configure + build + test. Lit auto-discovers the new project.
cmake -G Ninja -B build-noasan ...
cmake --build build-noasan --target check-audited
```

ZERO edits to `lib/Driver/`, `tools/nslc/`, or any infrastructure
file. The new project's cells appear in lit output automatically.

---

## 8. Common pitfalls

- **"Verilator not found"** inside the container ⇒ you're on `:dev`,
  not `:dev-m7`. Re-pull and re-run.
- **"riscv32-unknown-elf-gcc: command not found"** when running
  `rv32x_dev` or `turboV` cells ⇒ same as above.
- **`PROVENANCE.md missing required key`** at configure time ⇒
  the audited-corpus lint is rejecting a missing field. Add the
  missing `Upstream-SHA:` / `License:` / etc. line and reconfigure.
- **`Forbidden submodule under test/audited/`** ⇒ you accidentally
  ran `git submodule add` for one of the audited projects. Remove
  the submodule + the `.gitmodules` entry; re-vendor by file-copy.
- **`Forbidden nslc invocation in golden/REGEN.md`** ⇒ the
  regeneration recipe references `nslc`. That's a self-referential
  golden — replace the recipe with an external-source path
  (upstream NSL toolchain output OR hand-traced reference).
- **VCD divergence on `$date` only** ⇒ should NOT happen; `vcd_diff.py`
  ignores `$date`. If you see this, your `vcd_diff.py` is the
  upstream `vcd-diff` binary, not the vendored Python helper. Use
  the vendored helper.
- **Audited-corpus regression times out at 15 min** ⇒ one specific
  cell is slow. Check `build-noasan/test/audited/<project>/<sim>/<scenario>.log`
  for per-phase timings; if a project genuinely needs more than
  5 min individually, document the runtime in `golden/REGEN.md`.

---

## 9. Where to look for more detail

- **CLI contract for `-emit=verilog`**: `contracts/driver-emit-verilog.contract.md`
- **Stock-CIRCT pass pipeline (FSM→SV, Seq→SV, prepare-for-emit)**:
  `contracts/circt-passes.contract.md`
- **P-VEN + P-VCD + regression layout + lint policy**:
  `contracts/audited-corpus.contract.md`
- **`vcd_diff.py` CLI + parser + comparison policy**:
  `contracts/vcd-diff.contract.md`
- **Container surface (`:dev-m7`)**:
  `contracts/container-m7.contract.md`
- **Why each plan-time decision was made**: `research.md` §§1–9.
- **In-memory + on-disk entity catalogue**: `data-model.md`.
- **Acceptance criteria (8 SCs)**: `spec.md` § Success Criteria.
- **Clarifications session record**: `spec.md` § Clarifications.
