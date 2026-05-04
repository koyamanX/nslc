<!-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception -->

# `test/Fmt/` — `nsl-fmt` regression corpus

**Milestone**: T2 — Formatter v0
**Branch**: `010-t2-formatter-v0`
**Spec**: [`specs/010-t2-formatter-v0/`](../../specs/010-t2-formatter-v0/)

This directory holds the lit + FileCheck regression fixtures for
`nsl-fmt`. At Phase 1 it is intentionally empty save for this README;
the full corpus is enumerated in
[`tasks.md`](../../specs/010-t2-formatter-v0/tasks.md) Phase 3+ and
data-model.md §9.

## Conventions (inherited — NO per-directory config needed)

The project's lit infrastructure uses ONE root config at
[`test/lit.cfg.py`](../lit.cfg.py) which auto-discovers every
`.test`/`.nsl`/`.mlir` file under `test/` recursively. Per the design
note at the top of that file:

> Adding a new fixture is `cp some.test test/<Layer>/some.test` —
> no config edits.

This is why T009 ("create `test/Fmt/lit.cfg.py`") and T010
("register `test/Fmt/` in root lit.cfg.py") from the original task
plan are deliberate no-ops at Phase 1: dropping a fixture in here
just works.

If a future need arises for `Fmt/`-specific lit substitutions or
suffixes (e.g. a `%nsl-fmt` substitution), add them via a
`test/Fmt/lit.local.cfg` (the `.local` form is the project's
documented extension hook — see `test/Lower/` for precedent once
M5+ lands).

## Planned sub-tree (per `data-model.md` §9 + `tasks.md` Phase 3+)

```
test/Fmt/
├── rules/                  # FR-009 / FR-020 — six §5.3 rules
│   ├── alt-case-alignment/
│   ├── struct-member-alignment/
│   ├── proc-name-arg-wrap/
│   ├── bit-slice-spacing/
│   ├── operator-spacing/
│   └── attached-comments/
├── cli/                    # FR-001..FR-007 + FR-003a CLI surface
│   ├── stdin/
│   ├── in-place/
│   ├── check-mode/
│   ├── range/
│   ├── multi-file/
│   └── mutually-exclusive/
├── config/                 # FR-013..FR-016
├── directives/             # FR-012a — preprocessor pass-through
├── edge/                   # spec edge cases
└── idempotence/            # FR-021 + SC-002
    ├── audited/            # UNSUPPORTED: until M7 (P-VEN)
    └── synthetic/
```

Each fixture is a paired `pre.nsl` / `post.nsl` golden + a `.test`
RUN-line + an `idempotence.nsl` round-trip assertion.
