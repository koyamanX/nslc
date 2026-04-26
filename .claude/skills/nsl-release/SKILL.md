---
name: "nsl-release"
description: "Tag a release, run license audit, verify PROVENANCE.md across audited projects, and publish CI-built reproducible artifacts (no human-built) — gates M9 and any subsequent patch release."
argument-hint: "Release task (e.g., 'cut 1.0.0' or 'verify PROVENANCE for SDRAM_Controler')"
metadata:
  author: "nslc-project"
user-invocable: true
disable-model-invocation: false
---

## User Input

```text
$ARGUMENTS
```

You **MUST** consider the user input before proceeding (if not empty).

## Role

Owns the **M9 1.0.0 release** and any subsequent patch release. Constitution **Principle IX "Release artifacts" clause** is explicit: tagged releases MUST publish reproducible binaries and source tarballs **from CI**. **No human-built artifacts** are attached to releases.

The 1.0.0 milestone gates on:
- M8 done (riscv-formal integration; per Principle VI's formal clause)
- License audit complete; LLVM-Exception scope verified
- `PROVENANCE.md` per audited project verified
- CI release pipeline green
- Binary reproducibility check passes

## Outline

1. **Pre-flight checks.** Before cutting a release, verify all merge-gate signals are green for the release branch:
   - All M-track milestones up to and including the target are done (M8 for 1.0.0; lesser sets for `0.x` patches)
   - CI pipeline green on the release branch (Principle IX, all six stages including Formal once M8 is in)
   - All seven audited projects pass their golden-VCD regression (Principle VI end-to-end NON-NEGOTIABLE)
   - `rv32x_dev` passes the riscv-formal suite (1.0.0 only; for `0.x` series the formal gate may be explicitly disabled in release notes)

2. **License audit.**
   - Verify every file in the project carries an SPDX identifier (run the M0 SPDX-header check across the tree)
   - Verify the Apache-2.0 WITH LLVM-exception license applies; the LLVM-Exception scope means Verilog produced *by* the compiler is unencumbered
   - Verify every audited project's `PROVENANCE.md` cites a license compatible with Apache-2.0 WITH LLVM-exception
   - Verify riscv-formal license compatibility (M8 dependency)

3. **PROVENANCE.md verification.** For each of the seven audited projects under `test/audited/`:
   - `PROVENANCE.md` exists and records: upstream URL, commit SHA, license
   - Vendoring matches the recorded SHA (no drift from upstream)
   - License is restated correctly
   - Each project's `golden/REGEN.md` is present and accurate (P-VCD)

4. **Tag the release.**
   - Semantic versioning: `MAJOR.MINOR.PATCH`
   - 1.0.0 is the canonical release after M9 conditions are met
   - For `0.x` (formal-disabled) releases: clearly state the formal-clause carve-out in release notes
   - Tag MUST be signed (GPG); push the tag through normal channels — do not bypass push hooks

5. **Build artifacts in CI.** Per Principle IX:
   - Source tarball: produced from a fresh CI build on the tagged commit
   - Binaries: produced from CI matrix (Debug + Release on Linux x86_64 minimum)
   - **No human-built artifacts.** If you find yourself running `cmake --build` locally to attach to a release, stop — go fix the CI release pipeline instead.

6. **Binary reproducibility check.** Per Principle V (and Principle IX's reproducibility clause):
   - Run the release pipeline twice from the tagged commit
   - Verify byte-identical artifacts (tarball + binaries) across the two runs
   - A divergence means non-determinism leaked into the build; treat it as a release blocker

7. **Publish.**
   - GitHub Release page with: release notes, signed tag, CI-built artifacts attached
   - Release notes include:
     - Constitution version in effect (e.g., v1.3.0 / v1.4.0)
     - Audited-project PROVENANCE digest (URLs + SHAs + licenses)
     - Test-suite + formal-suite pass status
     - For `0.x` releases: explicit formal-clause carve-out

8. **Post-release.**
   - Open the next milestone in `README.md` §Roadmap if appropriate
   - If the release coincides with a constitution amendment (e.g., v1.4.0 narrowing of Linear scope), coordinate with `/speckit-constitution` so the SIR is in sync

9. **Verify.** Confirm:
   - [ ] All merge-gate signals green on the tagged commit
   - [ ] License audit complete; LLVM-Exception scope verified
   - [ ] Every audited project's `PROVENANCE.md` matches reality
   - [ ] Tag signed (GPG) and pushed via normal channels
   - [ ] No human-built artifact attached
   - [ ] Binary reproducibility verified across two CI runs
   - [ ] Release notes document constitution version, audit digest, test/formal pass status
   - [ ] If `0.x` formal-disabled release: carve-out documented in notes

## Constitutional anchors

- **Principle V** — Inspectable, Deterministic Pipeline (binary reproducibility)
- **Principle VI** — Layered Test Discipline; formal clause (rv32x_dev gate for 1.0.0)
- **Principle IX** — Continuous Integration & Delivery; **Release artifacts clause**: "Tagged releases MUST publish reproducible binaries and source tarballs from CI; no human-built artifacts are attached to releases."
- **Build, Code, and Licensing Standards** — License audit; SPDX coverage
