<!-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception -->

# Contract: T5 Configuration Resolution — TOML Precedence

**Branch**: `011-t5-lsp-formatting` | **Date**: 2026-05-12
**Anchors**: spec FR-005, FR-005a, FR-005b, FR-005c; SC-005

This contract freezes the FR-005 family TOML-precedence
behaviour — exactly how `nsl::fmt::Configuration` is computed
for each format request the LSP server receives. Resolved per
Session 2026-05-12 (Q1 — TOML wins; Q4 — malformed TOML falls
back to defaults + diagnostic).

Companion contracts:

- [`formatting-api.contract.md`](./formatting-api.contract.md)
  — wire-level behaviour that consumes the resolved
  `Configuration`.
- [`text-edit-shape.contract.md`](./text-edit-shape.contract.md)
  — TextEdit encoding that consumes `FormatResult.formattedText`.

---

## §1 Inputs (read each request)

| Input | Source | Notes |
| ----- | ------ | ----- |
| `params.textDocument.uri` | LSP request | Parsed to extract the document's parent directory. URIs with a non-`file:` scheme are special-cased — see §3. |
| Filesystem state | `discover_config` | Walks upward from the parent dir; reads zero or more `stat()` calls and at most one `.nsl-fmt.toml` file. |
| Built-in defaults | `nsl::fmt::default_configuration()` | Constexpr-evaluated; matches T2 contract §3 default field initialisers. |
| LSP `params.options` | LSP request | Read off the wire (required by LSP 3.16 schema) and **discarded**. NOT an input to the resolver. |

---

## §2 Resolution procedure (frozen)

```text
function resolveConfiguration(documentURI, sourceManager):
    parentDir = parentDirOf(documentURI)

    if scheme(documentURI) != "file":
        # §3 non-file URI fallback
        return ResolvedConfiguration{
            config         = default_configuration(),
            tomlPath       = nullopt,
            tomlDiagnostics = [],
            tomlFallback   = false,
        }

    tomlPath = nsl::fmt::discover_config(parentDir)

    if tomlPath is nullopt:
        # §4 no TOML discovered
        return ResolvedConfiguration{
            config         = default_configuration(),
            tomlPath       = nullopt,
            tomlDiagnostics = [],
            tomlFallback   = false,
        }

    # §5 TOML discovered — read it and parse
    tomlBuffer = readFile(*tomlPath)
    if read failed:
        # I/O error — treat as malformed
        return ResolvedConfiguration{
            config         = default_configuration(),
            tomlPath       = *tomlPath,
            tomlDiagnostics = [synthesized I/O diagnostic],
            tomlFallback   = true,
        }

    fileID = sourceManager.createFileID(tomlBuffer, *tomlPath)
    parsedConfig = default_configuration()
    result = nsl::fmt::parse_config_file(tomlBuffer, fileID, &parsedConfig)

    if result.status == Success:
        return ResolvedConfiguration{
            config         = parsedConfig,
            tomlPath       = *tomlPath,
            tomlDiagnostics = [],   # T2 may emit warnings on Success; surface them
                                    # via the same side channel if non-empty
            tomlFallback   = false,
        }

    # result.status ∈ {Refused, Error} — §6 malformed-TOML fallback
    return ResolvedConfiguration{
        config         = default_configuration(),
        tomlPath       = *tomlPath,
        tomlDiagnostics = result.diagnostics,
        tomlFallback   = true,
    }
```

The procedure runs **once per format request** (FR-005b — no
cross-request caching at T5).

---

## §3 Non-`file:` URI scheme (FR-005a)

When `documentURI` uses a scheme other than `file:` (e.g.,
`untitled:` for unsaved VS Code buffers, `vscode-vfs:` for
remote workspaces), the server MUST skip `discover_config`
entirely and use `default_configuration()`. The
`tomlFallback` flag is `false` — there is no malformed TOML
to surface.

Rationale: there is no filesystem path to walk upward from. A
synthesized "current working directory" walk would be
non-deterministic (it depends on where the server was started)
and would couple LSP behaviour to the launch environment in a
way Principle V (determinism) forbids.

---

## §4 No TOML discoverable (FR-005)

When `discover_config` returns `nullopt`, the server uses
`default_configuration()`. The `tomlFallback` flag is `false`.
The format request proceeds; no side-channel diagnostic is
emitted. Per the FR-005 resolution (Session 2026-05-12 Q1),
the LSP `params.options` is NOT consulted as a fallback —
defaults are authoritative.

---

## §5 TOML discovered, parses cleanly (FR-005)

When `parse_config_file` returns `Status::Success`, the parsed
`Configuration` is used verbatim. The `tomlFallback` flag is
`false`. SC-005's byte-equivalence with `nsl-fmt --stdin` holds
because both the CLI and the LSP server reach the same
`Configuration` value through identical T2 API calls.

`parse_config_file` MAY return `Status::Success` together with
non-empty `diagnostics` (warnings — e.g., unrecognised key per
T2 FR-015). In that case, the server MUST still surface those
warnings via the side-channel `publishDiagnostics` against the
TOML URI (same path as §6), so the user sees them in the
problems panel. The `tomlFallback` flag remains `false`
because the parsed configuration is used.

---

## §6 TOML discovered, malformed (FR-005c — Session 2026-05-12 Q4)

When `parse_config_file` returns `Status::Refused` (syntactic
TOML error) or `Status::Error` (semantic error — invalid value,
range violation, type mismatch, unknown key promoted to error)
— OR when the filesystem read of the TOML fails — the server
MUST:

1. Use `default_configuration()` as the resolved configuration
   for the current format request.
2. Set `tomlFallback = true`.
3. Capture `result.diagnostics` (or the synthesized I/O
   diagnostic, for read failures).

The caller (the format handler) is then responsible for §7's
side-channel emission **before** sending the format response.

---

## §7 Side-channel diagnostic emission (FR-005c)

When `tomlFallback == true` AND `tomlDiagnostics` is non-empty,
the format handler MUST emit one `textDocument/publishDiagnostics`
notification:

```json
{
  "jsonrpc": "2.0",
  "method": "textDocument/publishDiagnostics",
  "params": {
    "uri": "file:///path/to/.nsl-fmt.toml",
    "diagnostics": [
      {
        "range": { ... },
        "severity": <Error | Warning | Information | Hint>,
        "code": "<diagnostic-id>",
        "source": "nsl-fmt",
        "message": "<formatted diagnostic message>"
      },
      ...
    ]
  }
}
```

Constraints:

- `params.uri` is the **TOML file's** `file://` URI, NOT the
  .nsl document's URI.
- `params.version` is **omitted** (T5 does not track TOML
  versions; the TOML is read fresh per request).
- Each `diagnostics[i]` is the result of mapping a
  `basic::Diagnostic` via the existing T3 `toLspDiagnostic(...)`
  helper.
- `diagnostics[i].source` is set to `"nsl-fmt"` to disambiguate
  origin from `"nsl-sema"` and `"nsl-parse"` (T3-defined).

The notification MUST be sent **before** the format response so
the editor's problems panel updates before the user sees their
formatted output. Order matters: a client that processes
notifications in stream order will surface the TOML error in
its problems panel as soon as it receives the notification; the
subsequent format response then arrives and the editor applies
the (default-configured) edits.

The notification MUST be sent on every malformed-TOML format
request, even if the same TOML produced the same diagnostics on
a previous request. (This is a corollary of FR-005b's no-cache
rule.) The editor's diff against its existing diagnostics view
is a client-side concern.

When the TOML is later fixed (and the next format request runs
`parse_config_file` with `Status::Success`), the server MUST
emit a "clear" notification — a `publishDiagnostics` against
the TOML URI with an empty `diagnostics` array — to remove the
stale TOML diagnostics from the editor's problems panel. The
clear is emitted in the same place §7 emits the populated
notification (before the format response).

---

## §8 Determinism axes (Principle V)

The resolver is deterministic **except** for the
`discover_config` call, which is T2's documented filesystem-
dependent exception (`format-api.contract.md` §4 / §6).

CI fixtures that need determinism control bypass discovery by
ensuring the test document URI is in a directory tree with a
known TOML present (or absent). The integration-test harness
(`LspSession` helper) sets up a per-test temporary directory
and writes the fixture's NSL file and (optional) TOML there,
guaranteeing `discover_config` returns the expected result.

The fallback-on-malformed-TOML path (§6) is deterministic: the
same malformed TOML byte sequence produces the same
`Status::Refused` / `Status::Error` diagnostics from T2's
`parse_config_file` (T2 contract §6 forbids `unordered_map` /
`unordered_set` / time sources / etc. inside `libNslFmt.a`).
The side-channel notification emission is therefore also
deterministic.

---

## §9 Spec cross-reference

| Spec FR / SC | This contract section |
|---|---|
| FR-005 | §2, §4 (TOML wins resolution flow) |
| FR-005a | §2, §3 (non-file URI handling) |
| FR-005b | §2 (no caching — runs per request) |
| FR-005c | §6, §7 (malformed-TOML fallback + diagnostic) |
| SC-005 | §5 (Success path produces same `Configuration` as `nsl-fmt --stdin`) |
| Principle V | §8 (determinism axes) |
| Principle VII | §6 reuses the T3 diagnostic-mapping seam |
