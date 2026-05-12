<!-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception -->

# `toml++` — vendored third-party dependency

**Vendored at**: 2026-05-04 (T2 milestone, branch `010-t2-formatter-v0`)

## Upstream

- **Project**: `toml++` (a header-only C++17 TOML parser)
- **Author**: Mark Gillard
- **Repository**: <https://github.com/marzer/tomlplusplus>
- **Release tag**: `v3.4.0`
- **Commit SHA**: `30172438cee64926dc41fdd9c11fb3ba5b2ba9de`
- **License**: MIT

## Files vendored

| File | sha256 | Purpose |
|---|---|---|
| `toml.hpp` | `6b5172ad4dd6519aec67b919181fa7a38a2234131e5b2afa232dfe444819783e` | Single-header TOML v1.0.0 parser implementation (17 748 lines) |
| `LICENSE` | `529bc3900a9571e49db285b0df432397e70b881cc3bf48de6667ae74ff4b06d8` | Upstream MIT license text |

## Why vendored (not git submodule, not configure-time fetch)

Per Constitution Principle V (reproducibility / determinism), the
build MUST NOT fetch from the network at configure time or build
time. Per the same principle's vendoring discipline (and the
vendoring pattern already used for the seven audited NSL
projects under `test/audited/`), third-party dependencies are
copied into the repository in a one-time human action, with this
file recording the upstream URL + commit SHA + license for
licence-audit at M9.

`toml.hpp` is a single header (~485 KB), so vendoring is the
cleanest mechanism — no submodule pinning, no build-system
surgery beyond a one-line `add_subdirectory(third_party/tomlpp)`
in the project-root `CMakeLists.txt`.

## Updating to a newer toml++

1. Clone the upstream repo at the new tag in a temporary
   location (do NOT clone into the project tree):
   ```bash
   git clone --depth 1 --branch <new-tag> \
       https://github.com/marzer/tomlplusplus.git "$TMPDIR/tomlpp-vendor"
   ```
2. Capture the new commit SHA and `sha256sum` of `toml.hpp`
   and `LICENSE`.
3. Copy `toml.hpp` and `LICENSE` over their existing copies in
   `third_party/tomlpp/`.
4. Update this `PROVENANCE.md` with the new tag / SHA / sums.
5. Re-run the full project test suite to confirm no regression.
6. Open a single-purpose PR titled
   `third_party/tomlpp: vendor v<new-tag>` so the licence audit
   is easy to review.

## Licence compatibility

`toml++` is MIT-licensed; the project licence (Apache 2.0 WITH
LLVM-exception) is compatible with MIT in both directions. The
M9 release-audit gate (Constitution Principle IX § Release
artifacts) picks up this `PROVENANCE.md` automatically — no
extra configuration needed at release time.
