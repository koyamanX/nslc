<!-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception -->

# Branch protection (canonical settings)

This is the human-readable companion to
[`.github/branch-protection.json`](./branch-protection.json). When the
two disagree, **the JSON is authoritative** — it is what
`scripts/apply_branch_protection.sh` POSTs to the GitHub REST API.

## What is enforced

Branch `main` has the following protections, applied via
`gh api repos/<owner>/<repo>/branches/main/protection`:

| Setting | Value | Why |
|---|---|---|
| `required_status_checks.contexts` | 7 entries | The four `build-matrix` cells (`Debug × Release × {gcc, clang}` per spec Q2) plus stages 2–4 (`static-checks`, `unit-and-layer-tests`, `lowering-tests`). |
| `required_status_checks.strict` | `true` | PR branches must be up to date with `main` before merge. |
| `enforce_admins` | `true` | **The mandatory clause from spec Q3.** Branch protection MUST apply to repo admins too; without this, an admin merge silently bypasses every guarantee Constitution Principle V/VI/VII/VIII makes. |
| `required_pull_request_reviews.dismiss_stale_reviews` | `true` | A review approving an old commit should not transitively approve a force-push to a new commit. |
| `required_pull_request_reviews.required_approving_review_count` | `1` | One human review required, in addition to CI green. |
| `allow_force_pushes` | `false` | Force-pushes to `main` would void Principle V's "byte-identical artifacts at the same ref" guarantee since refs would change content. |
| `allow_deletions` | `false` | `main` cannot be deleted. |
| `required_conversation_resolution` | `true` | Unresolved review threads block merge. |

## What is NOT enforced (and why)

- **Stages 5 (`end-to-end`) and 6 (`formal`) are NOT in
  `required_status_checks.contexts`** — they ship `if: false` in
  `.github/workflows/ci.yml` until M7 / M8 land (FR-015). Adding them
  to `contexts` while they remain skipped would block every PR on a
  never-firing check (research §8). They become required in a one-line
  PR at M7 and M8.
- **CodeRabbit / external review tools** are not in the required-
  checks list. Their feedback is advisory, not a merge gate;
  [`CONTRIBUTING.md` §6](../CONTRIBUTING.md) says the same.

## The only permitted bypass (spec Q3)

When CI is red and merge is unavoidable, the **only** allowed bypass
is GitHub's repo-admin **"Merge without waiting for required status
checks to succeed"** override (which the `enforce_admins: true`
setting still permits because admins always retain that escape valve
on the GitHub API surface). Using it MUST be accompanied by a
**named-reason note in the PR description** so the audit trail is
preserved.

The following are NOT acceptable bypass mechanisms:

- `git commit --no-verify` / `--no-gpg-sign` (bypasses local hooks
  only; `enforce_admins` prevents them from short-circuiting the
  remote required-checks list).
- A `git push --force` to `main` — `allow_force_pushes: false`
  rejects this.
- A maintainer-comment-only override (e.g., "/skip-ci"). Linear or
  CodeRabbit conventions for ad-hoc bypass have no effect on the
  GitHub branch-protection layer.

## Applying the configuration

```bash
gh auth login                            # one-time
./scripts/apply_branch_protection.sh     # idempotent
```

The script reads `.github/branch-protection.json` and PUTs it via
`gh api`. Re-running surfaces "no changes" if the protection is
already in the desired state.

## When to update this file

Any edit to `.github/branch-protection.json` MUST land in the same
commit as a corresponding update to this document. Rationale: the
`enforce_admins` and named-reason-bypass clauses are constitutional
(Principle IX); silently changing them would violate Principle VII
(spec ↔ design coupling). Updates to this file are therefore subject
to the standard PR-review process and `nsl-constitution-review`
agent gating.
