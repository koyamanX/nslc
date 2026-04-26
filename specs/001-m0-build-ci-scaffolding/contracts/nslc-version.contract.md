<!-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception -->

# Contract: `nslc --version` CLI

**File**: `tools/nslc/main.cpp` (≤ ~60 lines per Principle II)
**Plan**: [../plan.md](../plan.md) §summary, §research §5
**Spec FRs covered**: FR-005 (driver thin entry), FR-006 (version output), SC-002 (regex + perf)
**Spec clarification**: Q5 (version-string format)

## Invocation

```
nslc --version
```

(Also accept `-v` and `-V` as aliases, per LLVM convention; not
required at M0 but reserves the surface.)

## Output contract

| Channel | Content |
|---|---|
| stdout | Exactly one line: `nslc <git-describe>\n` |
| stderr | Empty |
| exit code | `0` |

Where `<git-describe>` is the result of
`git describe --tags --always --dirty` injected at CMake configure
time (research §5). Concrete forms:

| Repo state | Output (stdout) |
|---|---|
| Pre-tag, clean | `nslc 0.0.0-dev+g<sha>\n` (the `0.0.0-dev` is a literal fallback emitted when no tag exists) |
| Pre-tag, dirty working tree | `nslc 0.0.0-dev+g<sha>-dirty\n` |
| At a tag (clean) | `nslc <tag>\n` (e.g., `nslc v1.0.0`) |
| Post-tag commits, clean | `nslc <tag>-<n>-g<sha>\n` |
| Post-tag commits, dirty | `nslc <tag>-<n>-g<sha>-dirty\n` |
| Source-tarball build (no `.git/`) | `nslc unknown\n` (CMake fallback when `Git_FOUND` is false) |

## Test contract (lit + FileCheck — TDD seed)

`test/Driver/version.test`:

```
RUN: %nslc --version | FileCheck %s
CHECK: {{^nslc [0-9A-Za-z._+-]+$}}
```

The `%nslc` substitution is defined in `test/lit.cfg.py` and points
at the configured build's `${NSLC_BINARY_DIR}/bin/nslc`.

Per Principle VIII, this test is committed and observed failing
(no binary yet) before `tools/nslc/main.cpp` and the `cmake/NSLVersion.cmake`
helper land.

## Performance contract

| Metric | Target | Source |
|---|---|---|
| Wall-clock latency | < 100 ms on the reference Linux x86_64 host | spec SC-002 |
| Memory peak | (no requirement) | — |
| stdout encoding | UTF-8 (LLVM standard); pure ASCII at M0 | LLVM convention |

## Implementation skeleton (illustrative)

```cpp
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// tools/nslc/main.cpp — nslc driver entry point.
// Per Constitution Principle II this file MUST stay ≤ ~60 lines and
// delegate behavior to nsl-driver. At M0 the only behavior is --version.

#include "nsl/Driver/Version.h"   // configure-time-generated header from cmake/NSLVersion.cmake
#include <cstdio>
#include <cstring>
#include <cstdlib>

int main(int argc, char **argv) {
  if (argc == 2 && (std::strcmp(argv[1], "--version") == 0 ||
                    std::strcmp(argv[1], "-v") == 0 ||
                    std::strcmp(argv[1], "-V") == 0)) {
    std::printf("nslc %s\n", NSLC_VERSION_STRING);
    return 0;
  }
  std::fprintf(stderr, "nslc: usage: nslc --version  (M0 smoke; full CLI lands M1+)\n");
  return 2;
}
```

(60 lines max — current draft is ~15. Future M1+ growth replaces
the `else` branch with a delegate call into `nsl-driver` per
Principle II.)

## Error contract

| Invocation | exit | stderr |
|---|---|---|
| `nslc --version` | 0 | (empty) |
| `nslc -v` / `-V` | 0 | (empty) |
| `nslc` (no args) | 2 | `nslc: usage: nslc --version  (M0 smoke; full CLI lands M1+)` |
| `nslc <anything-else>` | 2 | same usage line |

Exit code `2` (not `1`) follows the GNU convention for "incorrect
usage." Reserves `1` for "ran but failed" once real compilation
exists (M1+).
