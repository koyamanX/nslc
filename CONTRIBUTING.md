<!-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception -->

# Contributing to nslc

This document provides guidance for contributors — including AI coding assistants — working on the **nslc** compiler project.

It adapts the principles in the [Linux kernel's *AI Coding Assistants*](https://docs.kernel.org/process/coding-assistants.html) document to nslc (compiler implementation, tooling code, tests, and documentation); the AI-attribution policy in §5 below applies project-wide.

> **Contributing to documentation?** See [`docs/CLAUDE.md`](docs/CLAUDE.md) for the routing guide, the `docs/spec/` ↔ `docs/design/` coupling, the editing protocol, and the docs-specific PR checklist (§11).

---

## 1. Repository layout

```
nslc/                              ← project root (you are here)
├── README.md                      ← project intro + milestone roadmap (Mxx–Myy, Txx–Tyy)
├── CLAUDE.md                      ← milestone lookup tables (auto-loaded by Claude Code)
├── CONTRIBUTING.md                ← this file (project-wide policy)
├── LICENSE                        ← Apache 2.0 with LLVM Exceptions
├── .specify/                      ← Spec Kit memory, templates, and scripts
├── .claude/                       ← Claude Code skills used by this project
├── docs/                          ← language and tooling specifications
│   ├── CLAUDE.md                  ← docs/ routing guide + docs-specific PR checklist
│   ├── spec/                      ← authoritative grammar (EBNF)
│   └── design/                    ← compiler & tooling design
└── examples/                      ← example NSL programs (S/N/P-annotated)
```

The `docs/` tree contains the authoritative language grammar and the implementation design documents. Its internal structure, routing guide, editing rules, and docs-specific PR checklist live in [`docs/CLAUDE.md`](docs/CLAUDE.md).

The root-level `CLAUDE.md` holds the milestone-plan lookup tables (NSL-feature → milestone, tooling-feature → milestone) plus the Spec Kit routing markers (auto-loaded by Claude Code as project context).

---

## 2. Licensing

The nslc project — including everything in `docs/` — is licensed under **Apache License 2.0 with LLVM Exceptions**, the same license as LLVM, MLIR, and CIRCT. See [`LICENSE`](LICENSE) at the project root for the full text.

New files must carry an SPDX identifier on the first line. For Markdown:

```
<!-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception -->
```

For `.ebnf` files, use an EBNF comment:

```
(* SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception *)
```

For C++ / TableGen / build files, the LLVM convention applies:

```
//===----------------------------------------------------------------------===//
//
// Part of the nslc Project, under the Apache License v2.0 with LLVM Exceptions.
// See LICENSE for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
```

All contributions must be compatible with this license. The patent grant in Apache 2.0 §3 and the LLVM Exception together mean (a) contributors grant a patent license alongside their copyright contribution, and (b) Verilog and other artifacts produced *by* the compiler are not encumbered by the compiler's own license.

---

## 3. Tooling and workflow

This section is the **operational playbook** for producing a change. The binding rules for each external integration (Linear, CodeRabbit, GitHub Issues mirror) live in [`.specify/memory/constitution.md`](.specify/memory/constitution.md) "External Integrations" — that file is the source of authority; this section explains how the integrations are used day to day.

Several tools below are referenced in the constitution and in §3.8 below but are **not yet fully configured** in this repo. Specifically `P-LIN` (Linear team) and `P-TS` (`/speckit-taskstoissues` + Linear's GitHub Issues mirror) are still pending — see §3.8 for the prerequisite work.

### 3.1 Tool stack and roles

| Tool | Layer | Role | Status |
|---|---|---|---|
| **Linear** | project mgmt | Canonical for **feature-track** work items (team `nslc`, project `nslc`; issue/branch prefix `NSLC-<N>`) | **MUST** per constitution; team prefix landed; PR-trailer convention pending first non-trivial PR |
| **GitHub Issues** | bug tracking | Canonical for **bug reports** (defects in merged behavior, regressions, user-reported failures); NOT mirrored to Linear | **MUST** per constitution |
| **Claude Code** | coding agent | Primary autonomous implementor (CLI / IDE) | In use |
| **GitHub Copilot** | IDE completion | Inline completion, lightweight refactor | Optional; not currently configured |
| **GitHub** | VCS / CI | PRs, Actions, branch protection | In use; full CI matrix lands with `P-CI` |
| **CodeRabbit** | code review | PR review + Claude Code feedback loop | **MUST** per constitution; GitHub App and Claude Code plugin not yet configured |

**Design principle.** Each layer keeps one explicit human review gate. Agents may chain across layers; humans must stop at the gate.

### 3.2 End-to-end workflow

#### Phase 1 — Planning (Linear for features, GitHub Issues for bugs)

1. **Feature-track work** — features, refactors, plan items — becomes a **Linear issue** in the `nslc` team / `nslc` project. Slack threads, hallway requests, and mailing-list asks for new behavior are all converted to Linear issues before work begins. Issue/branch prefix: `NSLC-<N>`.
2. **Bug reports** — defects in merged behavior, regressions, user-reported failures — are filed directly as **GitHub Issues** in this repo. Bugs are NOT mirrored to Linear (mirror config excludes the bug-report label).
3. Issue template fields (Linear): `Scope`, `Acceptance Criteria`, `References`, `Dependencies`, `Test Cases`. Bug reports (GitHub Issues): reproduction steps + expected/actual.
4. If `docs/spec/`, `docs/design/`, or the milestone plan ([`README.md`](README.md) §Roadmap, [`CLAUDE.md`](CLAUDE.md), §3.8/§3.9 of this file) is relevant, link it under `References`.
5. Direct push from a skill into Linear via Linear MCP is "future, not currently configured" per the constitution. Today the path for **feature-track tasks** is `/speckit-tasks` → GitHub Issues (with the feature-track label) → Linear's first-party GitHub mirror (gated by `P-TS`). Until `P-TS` lands, file the Linear issue manually. **Bugs do not go through this routing** — file them in GitHub Issues directly with the bug label.

```bash
# Once Linear MCP is enabled:
claude mcp add --transport http linear https://mcp.linear.app/mcp
# First-time OAuth: run `/mcp` inside Claude Code.
```

#### Phase 2 — Implementation plan (Claude Code)

1. Hand the Linear issue to Claude Code and have it produce an **implementation plan** before any code is written.
2. The plan must cover: blast radius, files to touch, test strategy, and consistency with `docs/spec/` / `docs/design/` / the milestone plan ([`README.md`](README.md) §Roadmap + [`CLAUDE.md`](CLAUDE.md)).
3. **Do not start implementation until a human explicitly approves the plan.** This is the planning-layer review gate.
4. On approval, Claude Code transitions the Linear issue to `In Progress`.

#### Phase 3 — Implementation (Claude Code + Copilot)

- **Claude Code** lands the change in small commits. Each commit message references the originating Linear issue ID for feature work (e.g. `Refs NSLC-123`) or the GitHub Issue number for bug fixes (e.g. `Refs #123`).
- **GitHub Copilot** is allowed for inline completion, test scaffolds, and small helpers — optional, user discretion.
- Anything load-bearing — API surfaces, data-model changes, new external dependencies — is escalated to a human, not decided by the agent.

#### Phase 4 — Self-review (CodeRabbit, pre-PR)

Run CodeRabbit locally **before** opening the PR. Once the official Claude Code plugin is configured:

```bash
claude plugin marketplace update
claude plugin install coderabbit
```

From inside Claude Code:

```text
/coderabbit:review uncommitted     # uncommitted changes
/coderabbit:review --base main     # diff against main
```

Loop *findings → Claude Code fix → re-review* until zero critical findings remain. Per the constitution: CodeRabbit findings are advisory **except** when they flag a constitution / spec / regression violation, in which case adoption is non-negotiable. Claude Code judges adoption for the rest (e.g. style suggestions that conflict with the project's existing patterns may be declined with a brief rationale in the PR description).

#### Phase 5 — Pull request (CodeRabbit GitHub App + remote review)

1. Branch name includes the originating issue ID — `nslc-123-add-state-coverage` for a Linear-tracked feature (lowercase prefix in branch name; uppercase `NSLC-123` in trailers and PR body), or `fix-123-<slug>` for a GitHub-Issue bug fix. Auto-linking between PR and issue depends on this.
2. Opening the PR triggers the **CodeRabbit GitHub App** (once configured) for summaries, diagrams, and inline comments.
3. Requesting additional changes via `@claude` mentions is supported once `claude-code-action` is configured in `.github/workflows/`.
4. For high-risk changes, the Anthropic multi-agent **Code Review for Claude Code** (`/ultrareview`) is available alongside CodeRabbit.
5. Merge requires (per constitution Principle IX): green CI, addressed CodeRabbit blocking findings, and the originating Linear issue referenced in the PR description or trailer.

#### Phase 6 — Post-merge

- A `Fixes NSLC-<N>` line in the PR description auto-closes the Linear issue.
- **Spec / design / milestone drift is forbidden.** Per constitution Principle VII (spec ↔ design coupling), broadened here to include milestones: a change that affects the language MUST update `docs/spec/`; a change to implementation architecture MUST update `docs/design/`; a change to delivery sequencing MUST update the milestone plan ([`README.md`](README.md) §Roadmap, [`CLAUDE.md`](CLAUDE.md), and §3.8/§3.9 of this file as applicable). All three updates land in the same PR as the code change — never a follow-up.

### 3.3 Application criteria by change size

| Change scope | Linear issue | Plan approval | CodeRabbit | Reviewer |
|---|---|---|---|---|
| New feature; cross-file refactor | **MUST** | **MUST** | **MUST** | CodeRabbit + human |
| Single fix (< 3 files, < 100 lines) | **MUST** | recommended | **MUST** | CodeRabbit + human |
| Bug fix / typo / dep bump | **MUST** | not required | **MUST** | CodeRabbit + human |
| Docs / comments | optional | not required | optional | human only |

### 3.4 Configuration files

Reconciled to the actual state of this repo. Only one of the four files below exists today; the rest are deferred to milestones below.

- **`CLAUDE.md`** (project root) — holds the milestone-plan lookup tables (NSL-feature → milestone, tooling-feature → milestone) and preserves the Spec Kit routing markers (`<!-- SPECKIT START --> … <!-- SPECKIT END -->`). Project conventions for Claude Code (Linear team prefix, branch strategy, coding standards) will be added once `P-LIN` lands.
- **`.github/copilot-instructions.md`** — **not yet present**. To be added when Copilot is enabled. The intent is to derive it from `CLAUDE.md` rather than maintain it by hand; the generator is not yet written.
- **`.coderabbit.yaml`** — **not yet present**. To be added when the CodeRabbit GitHub App is enabled. The first rule to add is a check that `docs/spec/` ↔ `docs/design/` ↔ milestone-plan coupling is preserved on any code change.
- ~~`specs/`~~ — **does not exist in this repo.** The Spec Kit bootstrap created `.specify/` (memory + scripts + templates) and `.claude/skills/speckit-*` instead; authoritative language specs live in [`docs/spec/`](docs/spec/), implementation design in [`docs/design/`](docs/design/), and the delivery roadmap in [`README.md`](README.md) §Roadmap + [`CLAUDE.md`](CLAUDE.md).

### 3.5 MCP servers

| Server | URL / command | Purpose | Status |
|---|---|---|---|
| Linear | `https://mcp.linear.app/mcp` | Issue operations | Future per constitution; not currently configured |
| GitHub | Official MCP, or `gh` CLI via `Bash` | PR / repo operations | `gh` CLI in use today |

### 3.6 Pitfalls and best practices

1. **CodeRabbit false positives.** Strong on TypeScript / Python; less aware of project-specific patterns. Per constitution: findings are advisory except when they flag constitution / spec / regression violations. Tell Claude Code in plain language: *"weigh adoption against existing patterns; do not silently rewrite working code to satisfy a stylistic suggestion."*
2. **Context bloat.** `.specify/`, `docs/spec/`, and `docs/design/` are large. Load only the relevant slice; use [`docs/CLAUDE.md`](docs/CLAUDE.md) as the routing guide. Run `/clear` between unrelated tasks.
3. **Spec / design / milestone drift.** Code changes that affect language semantics MUST update `docs/spec/` AND `docs/design/`; changes that affect delivery sequencing MUST update the milestone plan ([`README.md`](README.md) §Roadmap and [`CLAUDE.md`](CLAUDE.md)). Drift is a constitution Principle VII violation. Once `.coderabbit.yaml` exists, add a rule that flags any code-only PR touching parser/sema/lower areas.
4. **PR granularity.** One feature per PR. Reviewer load grows linearly only if PRs stay focused.
5. **Agent runaway.** Claude Code MUST present an implementation plan and wait for a human approval before writing code (see Phase 2).
6. **Secrets.** Never paste production credentials, API keys, or DB passwords into Claude Code or Copilot. Share `.env.example`-style structure only; the agent figures out the rest.

### 3.7 Authority and contradictions

The binding rules for these integrations live in [`.specify/memory/constitution.md`](.specify/memory/constitution.md), section "External Integrations" (around lines 421+). This `CONTRIBUTING.md` §3 is the operational playbook, not the source of authority. If a contradiction arises between this section and the constitution, **the constitution wins**; report the contradiction as a bug per §8 below.

### 3.8 Workflow project-enablement (P-LIN, P-TS)

Two project-enablement deliverables gate Linear / issue-tracking
workflows that touch tooling and process. They are listed here rather
than alongside the compiler-track external dependencies in
[`README.md`](README.md) §Roadmap (`P-CI`, `P-VEN`, `P-VCD`) because
the compiler does not depend on them.

| # | Deliverable | Gates |
|---|---|---|
| **P-LIN** | Linear team created (team `nslc`, project `nslc`, issue/branch prefix `NSLC-<N>` — **landed in Constitution v1.4.0**); PR-trailer convention used in first non-trivial PR (still pending). | First Linear-tracked work item (External Integrations) |
| **P-TS** | `/speckit-taskstoissues` operational; Linear's first-party GitHub-issue mirror configured. | First `/speckit-tasks` invocation (External Integrations) |

### 3.9 Updating the milestone plan

The milestone tables in [`README.md`](README.md) §Roadmap and the
roll-up tables in [`CLAUDE.md`](CLAUDE.md) §1–§2 are governed by these
rules:

- **Adding a new milestone, retiring one, or materially changing
  scope** requires a constitutional amendment if it affects a
  Principle's gate; otherwise it is a routine docs PR.
- **Adding a row to the language-feature roll-up in
  [`CLAUDE.md`](CLAUDE.md) §1 or to the tooling-feature roll-up in
  [`CLAUDE.md`](CLAUDE.md) §2** is mandatory whenever a corresponding
  spec or tool feature is added (Principle VII).
- **Renumbering `M*` or `T*`** is forbidden; retired numbers are not
  reused (mirrors the Principle I rule for `Sn`/`Nn`/`Pn`).
- **Cross-links from
  [`docs/design/nsl_compiler_design.md`](docs/design/nsl_compiler_design.md)
  and
  [`docs/design/nsl_tooling_design.md`](docs/design/nsl_tooling_design.md)**
  must be kept current — those files defer to the project-root
  milestone tables for delivery sequencing.

If a contradiction arises between the milestone plan and the
Constitution, the Constitution wins; report the contradiction as a
docs bug per §8 below. If a contradiction arises between the
milestone plan and a design doc on a *delivery sequencing* question,
the milestone plan wins. If the contradiction is on a *design*
question (architecture, semantics), the design doc wins.

### 3.10 Skill ↔ agent parallel-update protocol

Each project-specific implementation skill at
`.claude/skills/nsl-*/SKILL.md` has a parallel agent definition at
`.claude/agents/nsl-*.md`. Per Constitution Governance "Runtime
guidance" clause, **the two MUST be updated together** — editing
one without the other is forbidden. The agent file points to the
skill as the canonical source; if the skill's protocol changes, the
agent's "Operating rules" / tool set / hand-off list must be
re-checked for drift.

`/nsl-constitution-review` includes a coupling check on this pair
(skill present + agent present + both edited in the same PR when one
is touched).

### 3.11 Local CI reproduction

`scripts/ci.sh` is the single authoritative entry point for the
six-stage CI pipeline mandated by Constitution Principle IX. The
GitHub Actions workflow at `.github/workflows/ci.yml` calls into
the same dispatcher so divergence between local and remote runs is
impossible (FR-021).

```bash
./scripts/ci.sh build-matrix             # stage 1 (Release × host by default)
./scripts/ci.sh build-matrix --matrix    # stage 1, fan out all 4 cells
./scripts/ci.sh static-checks            # stage 2 (clang-format + clang-tidy + SPDX)
./scripts/ci.sh unit-tests               # stage 3 (ctest)
./scripts/ci.sh lowering-tests           # stage 4 (lit)
./scripts/ci.sh e2e                      # stage 5 — wired but empty until M7
./scripts/ci.sh formal                   # stage 6 — wired but empty until M8
./scripts/ci.sh all                      # stages 1..4 in order; stop at first fail
```

Stages 5 (`end-to-end`) and 6 (`formal`) deliberately exit 0 with a
"wired but empty" diagnostic until M7 / M8 land. They are not in
`.github/branch-protection.json` `required_status_checks.contexts`
yet — adding them is a one-line PR at each milestone.

The merge gate enforces required-checks for **all** PRs, including
those by repository administrators (`enforce_admins: true`). The
**only** permitted bypass is GitHub's repo-admin "Merge without
waiting for required status checks" override, accompanied by a
named-reason note in the PR description per Constitution Principle
IX. `git commit --no-verify` / `git push --force` to `main` /
maintainer-comment overrides are NOT acceptable bypass mechanisms.

---

## 4. Sign-off and certification

Contributors are expected to certify their own work. We follow the spirit (though not necessarily the formal machinery) of the [Developer Certificate of Origin](https://developercertificate.org/): by adding a `Signed-off-by` line to a commit, you certify that you have the right to submit the contribution under the project's license, and that the contribution is your own work or properly attributed.

Commit message form:

```
Brief summary of the change

Longer explanation if needed.

Signed-off-by: Your Name <your.email@example.com>
```

---

## 5. AI-assisted contributions

Two rules govern AI-assisted commits, both following the [Linux kernel's *AI Coding Assistants*](https://docs.kernel.org/process/coding-assistants.html) policy.

### 5.1 AI agents MUST NOT add `Signed-off-by`

Only a human can certify a contribution. The human submitter is responsible for:

- Reviewing all AI-generated content before committing
- Verifying that the change is consistent with `docs/spec/` (or, when the change is to `docs/spec/` itself, that it reflects a genuine, intentional language decision)
- Updating cross-references per `docs/CLAUDE.md` §8 when the change spans `docs/spec/` and `docs/design/`
- Ensuring the change complies with the project's license
- Adding their own `Signed-off-by` line and taking full responsibility for the contribution

If you cannot do all of the above, do not commit the change.

### 5.2 AI agents MUST add `Assisted-by`

When an AI tool meaningfully contributed to a change, add an `Assisted-by` trailer to the commit message. This helps track how AI is being used in the project.

Format:

```
Assisted-by: AGENT_NAME:MODEL_VERSION [TOOL1] [TOOL2]
```

Where:

- **`AGENT_NAME`** is the AI tool or framework (e.g. `Claude`, `Claude-Code`, `Cursor`, `Aider`)
- **`MODEL_VERSION`** is the specific model identifier where known (e.g. `claude-opus-4-7`, `gpt-5`)
- **`[TOOL_n]`** is optional and lists specialized analysis or validation tools that were also involved (e.g. an EBNF validator, a Mermaid renderer, a Markdown linter)

Basic editing tools (git, your editor, generic shell utilities) should **not** be listed.

### Examples

A grammar clarification done with help from an assistant:

```
spec/lang: clarify scope of state_name inside proc bodies (S11)

Make the proc-scoping rule explicit: state_name declared inside a proc
is not visible to other procs in the same module, mirroring the existing
behaviour in cpu16/turboV.

Also update the cross-reference in docs/CLAUDE.md §8.

Signed-off-by: Chihiro Koyama <chihiro@example.com>
Assisted-by: Claude:claude-opus-4-7
```

A design-doc edit using a static-analysis-style tool:

```
design/compiler: tighten ASTToMLIR ownership story

Use unique_ptr consistently for AST nodes; non-owning raw pointers for
symbol references resolved by Sema.

Signed-off-by: Chihiro Koyama <chihiro@example.com>
Assisted-by: Claude-Code:claude-opus-4-7 markdown-lint mermaid-validate
```

A change that was entirely human-written: omit the `Assisted-by` trailer.

---

## 6. Commit message style

- First line: `area: short summary` in imperative mood, ≤ 72 characters. Common areas: `spec/lang`, `spec/pp`, `design/compiler`, `design/tooling`, `docs`, `claude.md`, `readme`, `meta`. As compiler and tooling code lands, expect additional areas (`lib/parse`, `lib/sema`, `tools/nslc`, `test/…`, `cmake`, `ci`).
- Blank line.
- Body: explain the *why* and the *what was changed*, wrapped at ~72 characters.
- Trailers (in this order, at the bottom of the message): `Signed-off-by`, then `Assisted-by` (if applicable), then `Linear: NSLC-<N>` (if the commit references a Linear issue), then any `Reviewed-by` / `Acked-by`.

---

## 7. Pull-request checklist

Before opening a PR, confirm:

- [ ] Changed only the sections needed for the task; no unrelated reformatting.
- [ ] Commit messages carry `Signed-off-by`, and `Assisted-by` where applicable.
- [ ] New files carry the appropriate SPDX identifier (§2).
- [ ] No upstream NSL PDF files were committed.

If your PR touches the `docs/` tree, also run through the documentation-specific checklist in [`docs/CLAUDE.md`](docs/CLAUDE.md) §11 (cross-reference updates, `Sn`/`Nn`/`Pn` quick-map, line-range freshness in §§4–7).

---

## 8. Reporting issues

For bugs, design questions, or policy questions about the project as a whole, open an issue with a clear description and (where applicable) the specific files and lines involved.

For contradictions or bugs inside the `docs/` tree — including `docs/spec/` vs. `docs/design/` mismatches and `docs/spec/` vs. upstream-PDF mismatches — see the reporting guidance in [`docs/CLAUDE.md`](docs/CLAUDE.md) §10–§11.
