<!-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception -->

# Phase 1 Data Model: T3 — `nsl-lsp` Skeleton

**Branch**: `010-t3-lsp-skeleton` | **Date**: 2026-05-05

This document catalogs the entities `nsl-lsp` introduces and how
they relate. Compared to the spec's "Key Entities" section (which
is stakeholder-facing), this document is implementation-facing and
records the C++17 shape, the existing `libNSLFrontend.a` types
that each entity wraps or consumes, and the lifetime / ownership
relationships.

---

## §1 LSP wire-protocol entities

These are JSON-shaped objects flowing across the JSON-RPC channel.
T3 implements only the subset listed below; the full LSP
vocabulary remains for later milestones.

### §1.1 `InitializeParams` (client → server, request)

| Field                          | Type            | T3 use                                                    |
| ------------------------------ | --------------- | --------------------------------------------------------- |
| `processId`                    | `int \| null`   | Logged at INFO; not otherwise consumed                    |
| `clientInfo.name`              | `string?`       | Logged at INFO; not otherwise consumed                    |
| `clientInfo.version`           | `string?`       | Logged at INFO                                            |
| `rootUri`, `rootPath`          | string / null   | Ignored at T3 — workspace-folder handling deferred to T9 |
| `workspaceFolders`             | array? / null   | Ignored at T3                                             |
| `capabilities.general.positionEncoding` | `string?` (3.17) | Ignored at T3 (LSP 3.16 floor; UTF-16 unconditionally)    |
| `capabilities.textDocument.synchronization.dynamicRegistration` | `bool?` | Ignored at T3 |
| `initializationOptions`        | any             | Ignored at T3; `NSL_INCLUDE` is the include-path source |
| `trace`                        | `string?`       | Ignored at T3 (logging level is `NSL_LSP_LOG_LEVEL`)      |

### §1.2 `InitializeResult` (server → client, response)

```json
{
  "capabilities": {
    "textDocumentSync": {
      "openClose": true,
      "change": 1,
      "save": false,
      "willSave": false,
      "willSaveWaitUntil": false
    },
    "foldingRangeProvider": true
  },
  "serverInfo": {
    "name": "nsl-lsp",
    "version": "<NSLC_VERSION_STRING>"
  }
}
```

The exact byte sequence is frozen by
[`contracts/lsp-protocol.contract.md`](./contracts/lsp-protocol.contract.md)
§1.2 and asserted in `lifecycle_test::CapabilitiesExact`.

### §1.3 `DidOpenTextDocumentParams`, `DidChangeTextDocumentParams`, `DidCloseTextDocumentParams`

| Notification                | Body                                            | T3 behavior                                                       |
| --------------------------- | ----------------------------------------------- | ----------------------------------------------------------------- |
| `textDocument/didOpen`      | `{textDocument: {uri, languageId, version, text}}` | Register `NslTU(uri)` with initial state; schedule parse + sema |
| `textDocument/didChange`    | `{textDocument: {uri, version}, contentChanges: [{text}]}` | Replace `NslTU(uri).contents` with `contentChanges[0].text`; schedule reparse. **`Full`-mode payload only**: any element with `range`/`rangeLength` is rejected per FR-006 |
| `textDocument/didClose`     | `{textDocument: {uri}}`                         | Emit one final empty `publishDiagnostics`; release `NslTU(uri)`   |

### §1.4 `PublishDiagnosticsParams` (server → client, notification)

```json
{
  "uri": "<document-uri>",
  "version": <int>,
  "diagnostics": [
    { "range": { … }, "severity": <int>, "code": "<S01|S02|N05|…>",
      "source": "<nsl-sema|nsl-parse|nsl-preprocess>",
      "message": "<formatted message>",
      "relatedInformation": [ … ] // only when include-from-notes apply
    },
    …
  ]
}
```

Diagnostics are sorted by `(range.start.line, range.start.character, severity)`
for determinism (Principle V; matches the existing
`DiagnosticEngine::renderAll` sort behavior).

### §1.5 `FoldingRangeParams`, `FoldingRange[]`

Request body: `{textDocument: {uri}}`. Response: an array of:

```json
{ "startLine": <int>, "endLine": <int>, "kind"?: "comment" }
```

Both `startLine` and `endLine` are zero-based per the LSP spec.
`startCharacter` / `endCharacter` are omitted (clients fold whole-line).
`kind` is set to `"comment"` for multi-line block-comment folds and
omitted for code-block folds (FR-016).

### §1.6 `CancelParams` (`$/cancelRequest`, both directions)

Body: `{id: <int|string>}`. Server-side: looks up the entry in the
in-flight-request table; signals its `CancellationToken` if present;
silently ignores otherwise (FR-020j).

---

## §2 Internal C++17 entities

### §2.1 `JSONTransport` (private to `lib/LSP/`)

```cpp
namespace nsl::lsp {
class JSONTransport {
public:
    JSONTransport(std::istream& in, std::ostream& out);

    // Returns std::nullopt on EOF. Throws TransportError on framing failure.
    std::optional<llvm::json::Value> readMessage();

    // Serializes msg with Content-Length framing. Thread-safe.
    void writeMessage(const llvm::json::Value& msg);

private:
    std::istream& in_;
    std::ostream& out_;
    std::mutex write_mtx_;
};
}
```

### §2.2 `NslLSPServer` (private to `lib/LSP/`)

LSP-protocol layer. Owns `JSONTransport`, the dispatch table
(method-name → handler), the in-flight request table (request-id
→ `CancellationToken`), and the `NslServer` it delegates to.

```cpp
namespace nsl::lsp {
class NslLSPServer {
public:
    NslLSPServer(JSONTransport& transport, NslServer& backend);

    int run();           // main message loop; returns process exit code

private:
    void onInitialize(const RequestId& id, const llvm::json::Value& params);
    void onInitialized();
    void onShutdown(const RequestId& id);
    void onExit();
    void onDidOpen(const llvm::json::Value& params);
    void onDidChange(const llvm::json::Value& params);
    void onDidClose(const llvm::json::Value& params);
    void onFoldingRange(const RequestId& id, const llvm::json::Value& params);
    void onCancelRequest(const llvm::json::Value& params);

    void publishDiagnostics(StringRef uri, int version,
                            llvm::ArrayRef<llvm::json::Value> diags);

    JSONTransport& transport_;
    NslServer& backend_;
    bool initialized_ = false;
    bool shutdown_received_ = false;
    InFlightTable in_flight_;
};
}
```

`InFlightTable` is a small `std::mutex`-guarded
`llvm::DenseMap<RequestId, std::shared_ptr<std::atomic<bool>>>`.

### §2.3 `NslServer` (private to `lib/LSP/`)

Language-logic layer. Stateless (modulo the `TUScheduler` it
owns). Provides operations the protocol layer dispatches to.

```cpp
namespace nsl::lsp {
class NslServer {
public:
    explicit NslServer(IncludeSearchPath include_paths);

    // didOpen / didChange land here; queues a parse + sema.
    void openOrUpdate(StringRef uri, int version, std::string contents);

    // didClose lands here.
    void close(StringRef uri);

    // Returns the latest published diagnostics for uri, or empty.
    void withDiagnostics(StringRef uri,
                        llvm::function_ref<void(int version,
                                                 llvm::ArrayRef<Diagnostic>)> cb);

    // foldingRange handler entry; cancellable.
    std::vector<FoldingRange> foldingRange(StringRef uri,
                                            CancellationToken token);

private:
    IncludeSearchPath include_paths_;
    TUScheduler scheduler_;
};
}
```

### §2.4 `TUScheduler` + `NslTU` (private to `lib/LSP/`)

Per
[`docs/design/nsl_tooling_design.md`](../../docs/design/nsl_tooling_design.md)
§3.3, with the public-API shape extended to expose the diagnostics
callback the protocol layer needs.

```cpp
namespace nsl::lsp {
class NslTU {
public:
    struct State {
        int version;
        std::string contents;
        std::unique_ptr<nsl::CompilationUnit> ast;
        std::vector<nsl::Diagnostic> diagnostics;
        std::unique_ptr<nsl::SymbolTable> symbols;
    };

    void reparse(int version, std::string contents,
                 const IncludeSearchPath& include_paths);
    void withState(llvm::function_ref<void(const State&)> fn);
    void cancelInFlight();    // for didClose

private:
    std::mutex mtx_;
    std::condition_variable cv_;
    std::optional<State> current_;
    int latest_version_ = -1;
    std::atomic<bool> dirty_{false};
};

class TUScheduler {
public:
    explicit TUScheduler(unsigned worker_count);

    void open(StringRef uri);
    void update(StringRef uri, int version, std::string contents,
                const IncludeSearchPath& include_paths);
    void close(StringRef uri);
    void withState(StringRef uri,
                   llvm::function_ref<void(const NslTU::State&)> fn);

    using DiagnosticsCallback = llvm::function_ref<void(StringRef uri,
                                                         int version,
                                                         llvm::ArrayRef<nsl::Diagnostic>)>;
    void setOnDiagnostics(DiagnosticsCallback cb);

private:
    llvm::StringMap<std::unique_ptr<NslTU>> tus_;
    std::mutex tus_mtx_;
    llvm::ThreadPool pool_;
    DiagnosticsCallback on_diagnostics_;
};
}
```

### §2.5 `CancellationToken` (private to `lib/LSP/`)

```cpp
namespace nsl::lsp {
struct CancellationToken {
    std::shared_ptr<std::atomic<bool>> flag;
    bool isCancelled() const noexcept {
        return flag && flag->load(std::memory_order_acquire);
    }
};
}
```

### §2.6 `IncludeSearchPath`

Read once from `NSL_INCLUDE` at server startup per FR-020a.

```cpp
namespace nsl::lsp {
struct IncludeSearchPath {
    std::vector<std::string> angle_paths;   // from NSL_INCLUDE; system separator
    // quote-form is resolved per-document via uri.parent_path()
};
}
```

### §2.7 `Logger` (stderr-only, plain text)

```cpp
namespace nsl::lsp {
enum class LogLevel : uint8_t { Error = 0, Warn = 1, Info = 2, Debug = 3 };

class Logger {
public:
    static void init(LogLevel min);  // reads NSL_LSP_LOG_LEVEL if set
    static LogLevel level();
    static void log(LogLevel lvl, llvm::StringRef msg);
};

#define NSL_LSP_LOG(LVL, ...) ::nsl::lsp::Logger::log(LVL, ::llvm::formatv(__VA_ARGS__).str())
}
```

Format: `<ISO-8601-ts> <LEVEL> <message>\n`. Stream is `std::cerr`,
flushed line-buffered.

### §2.8 `RequestId`

LSP request IDs are JSON-RPC: integer or string. Internally:

```cpp
namespace nsl::lsp {
using RequestId = std::variant<int64_t, std::string>;
}
```

---

## §3 Existing `libNSLFrontend.a` types consumed (no widening required)

| Type                              | Source header                                | T3 use                                                  |
| --------------------------------- | -------------------------------------------- | ------------------------------------------------------- |
| `nsl::SourceManager`              | `include/nsl/Basic/SourceManager.h`          | Resolves `SourceLocation` → `(file, line, col)` + `#line` |
| `nsl::SourceLocation`             | `include/nsl/Basic/SourceLocation.h`         | Carried by every diagnostic                             |
| `nsl::SourceRange`                | `include/nsl/Basic/SourceLocation.h`         | Carried by every AST node; mapped to LSP `Range`        |
| `nsl::Diagnostic`                 | `include/nsl/Basic/Diagnostic.h`             | Mapped to LSP `Diagnostic` per R5                       |
| `nsl::Severity`                   | `include/nsl/Basic/Diagnostic.h`             | Mapped to LSP `DiagnosticSeverity`                      |
| `nsl::DiagnosticEngine`           | `include/nsl/Basic/Diagnostic.h`             | Receives sema/parser/preprocessor output                |
| `nsl::CompilationUnit`            | `include/nsl/AST/CompilationUnit.h`          | Walked by `FoldingRangeBuilder`                         |
| `nsl::ast::ASTVisitor`            | `include/nsl/AST/ASTVisitor.h`               | Base class for `FoldingRangeBuilder`                    |
| `nsl::SymbolTable`                | `include/nsl/Sema/SymbolTable.h`             | Cached on `NslTU::State` for T4/T9 use; not consumed at T3 |
| `nsl::driver::EmitTokensOptions`  | `include/nsl/Driver/EmitTokens.h`            | Reuse the `include_paths` plumbing — `NslServer` sets the same fields when invoking the driver layer |
| `nsl::driver::Compilation`        | `include/nsl/Driver/Compilation.h`           | Run the parse + sema pipeline (existing M3 entry point) |

**No M-track API widening is required.** The `Diagnostic` struct
already exposes everything T3 needs (severity, loc, message, notes,
include-from flag); the `DiagnosticEngine` already exposes
`diagnostics()` for read access; the `Compilation` driver already
runs parse + sema on a buffer + include-paths input.

---

## §4 State transitions

### §4.1 Server-process lifecycle

```
┌─ created (entry to runStdioServer) ──► initialize(received)
│                                              │
│                                              ▼
│                                        initialize(responded)
│                                              │
│                                              ▼
│                                       initialized(received)
│                                              │
│                       ┌──────────────────────┼──────────────────────┐
│                       ▼                      ▼                      ▼
│                   didOpen N            didChange N           didClose N
│                       │                      │                      │
│                       ▼                      ▼                      ▼
│                   publishDiagnostics … (one per state change per uri)
│                                              │
│                                              ▼
│                                         shutdown(received)
│                                              │
│                                              ▼
│                                         shutdown(responded)
│                                              │
│                                              ▼
└─                                          exit ─────────► exit code 0
   exit before shutdown ────────────────────────────────────► exit code 1
```

### §4.2 Per-document `NslTU` state

```
                 reparse(v1, text1)
   not_open  ────────────────────► current{v1, ast1, diags1}
                                       │
                                       │ reparse(v2, text2)  (v2 > v1)
                                       ▼
                                   current{v2, ast2, diags2}
                                       │
                                       │ close()
                                       ▼
                                   not_open
```

If a `reparse(vK, …)` arrives while a worker is processing
`reparse(vJ, …)` with `vK > vJ`, the worker's result is discarded
once it finishes (FR-008); only the result for the latest version
is published.

### §4.3 In-flight request lifetime

```
    request received ────► token created ────► dispatch to handler
                                                       │
                              ┌────────────────────────┴────┐
                              ▼                              ▼
                        handler completes        $/cancelRequest received
                              │                              │
                              ▼                              ▼
                       send Response{result}      flip token; handler observes
                                                  at next polling point
                                                              │
                                                              ▼
                                                   send Response{error: -32800}
```

In both cases, the token is removed from the in-flight table after
the response is sent.
