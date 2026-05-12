<!-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception -->

# NSL Developer Tooling — Design Specification

**Tools covered:** Language Server (LSP), Syntax Highlighter, Code Formatter, Linter
**Implementation language:** C++17 (core) + TypeScript (VS Code extension shell)
**Shares:** Lexer / Preprocessor / Parser / Sema from the NSL compiler

---

## 1. Design Philosophy — One Front-End, Five Frontdoors

The NSL compiler (`nslc`) already owns the hard part: lexing, preprocessing, parsing, semantic analysis, and type-checking. Everything in this document is built on top of that front-end rather than duplicating it. Five binaries — the compiler driver plus four developer tools (`nsl-lsp`, `nsl-fmt`, `nsl-lint`, `nsl-opt`) — share **one libNSLFrontend.a**:

```
┌─────────────────────────────────────────────────────────────────────────┐
│                       libNSLFrontend.a (shared core)                    │
│                                                                         │
│   Preprocessor │ Lexer │ Parser │ AST │ Sema │ SymbolTable │ Diagnostic │
└─────────────────────────────────────────────────────────────────────────┘
       ▲               ▲               ▲               ▲              ▲
       │               │               │               │              │
┌──────┴─────┐  ┌──────┴──────┐  ┌─────┴──────┐  ┌─────┴─────┐  ┌────┴────┐
│    nslc    │  │   nsl-lsp   │  │  nsl-fmt   │  │  nsl-lint │  │ nsl-opt │
│ (→Verilog) │  │   (LSP)     │  │ (formatter)│  │ (linter)  │  │  (MLIR) │
└────────────┘  └─────────────┘  └────────────┘  └───────────┘  └─────────┘
                        │                                              
                   JSON-RPC (LSP)                                       
                        │                                              
              VS Code / Neovim / Emacs extension                       
                        │                                              
          ┌─────────────┴──────────────┐                               
          │                            │                               
 Tree-sitter grammar            TextMate grammar                       
 (primary highlighter)          (fallback highlighter                  
                                 for editors without                   
                                 tree-sitter support)                  
```

Consequences:

- **Zero semantic drift.** The formatter uses the same parser as the compiler; the linter uses the same symbol table; the LSP reports the same diagnostics the compiler would. A user never sees "the formatter accepts this but the compiler rejects it."
- **Incremental reparse is cheap.** The one shared AST is built with stable node identities so the LSP can diff old-vs-new after each keystroke.
- **Tool-specific code stays small.** Each tool is essentially a driver on top of the library: a few hundred to a few thousand lines.

---

## 2. Shared Infrastructure Enhancements

The library needs a few additions beyond what the compiler needs:

### 2.1 Incremental parsing

```cpp
class IncrementalParser {
public:
    // Full parse.
    std::unique_ptr<CompilationUnit> parse(SourceFile &file);

    // After the user edits [startLine..endLine], reparse only that region,
    // reattaching to the old AST where possible.  Keyed on the pre-edit
    // AST so that unaffected subtrees reuse pointers.
    std::unique_ptr<CompilationUnit> reparse(
        CompilationUnit    *oldAST,
        SourceFile         &newFile,
        std::vector<Edit>   edits);
};
```

Strategy: for large files this matters, but an initial implementation can do a full reparse — NSL files rarely exceed a few thousand lines.

### 2.2 Stable node IDs

Every AST node gets an opaque `NodeID` that's stable across reparses for the "same" syntactic construct. This lets the LSP send incremental diagnostics without the client flickering.

### 2.3 Token lattice for editor use

Alongside the AST, the parser emits a `TokenLattice` — a flat `std::vector<Token>` with classification tags (`Keyword`, `Identifier.Register`, `Identifier.Wire`, `Identifier.Proc`, `Identifier.State`, …). This is what the highlighter and formatter walk.

### 2.4 Concrete Syntax Tree (CST) layer

The AST throws away whitespace and comments. The **formatter** needs both. We add a thin CST (concrete syntax tree) that preserves trivia (spaces, tabs, newlines, comments) between tokens. The AST nodes carry cross-references into CST trivia nodes.

```cpp
class CSTNode {
public:
    NodeID id;
    TokenRange range;                          // [first, last] token indices
    std::vector<Trivia> leadingTrivia;         // comments, blank lines before
    std::vector<Trivia> trailingTrivia;        // trailing line comments
    std::vector<CSTNode*> children;
};

struct Trivia {
    enum class Kind { Whitespace, LineComment, BlockComment, Newline } kind;
    StringRef text;
    SourceLoc loc;
};
```

Only the formatter and LSP "format" request need the CST. Compiler/linter skip it.

---

## 3. Language Server — `nsl-lsp`

> **T3 status: delivered.** The four contracts under
> `specs/010-t3-lsp-skeleton/contracts/` (lsp-protocol,
> diagnostic-mapping, folding-range, lsp-test-harness) freeze the
> wire-visible behavior. The README §Roadmap row T3 test gate
> ("open a file with a Sema error, observe diagnostic; edit,
> observe re-diagnose") is materialized as
> `LifecycleSuite::README_TestGate_OpenErrorEditFix` and passes.
> 27 LSP integration tests run via `ctest -R lsp` in under 3
> seconds. T4/T5/T9/T10 extend the dispatch table in `lib/LSP/
> NslLSPServer.cpp` with new method handlers; the framing,
> lifecycle, and document-sync layers below do not change.

### 3.1 Overall Architecture

Modeled on clangd's three-layer design: ClangdLSPServer handles the LSP protocol details. Incoming requests are routed to some method on this class using a lookup table, and then implemented by dispatching them to the contained ClangdServer.

```
┌─────────────────────────────────────────────────────────────────────────┐
│                         JSONTransport  (stdin/stdout)                   │
│                         JSON-RPC framing per LSP spec                   │
└─────────────────────────────────────────────────────────────────────────┘
                                    ▲
                                    │
┌───────────────────────────────────┴─────────────────────────────────────┐
│                      NslLSPServer   (LSP protocol layer)                │
│   – parses JSON requests into typed structs (Protocol.h)                │
│   – handles lifecycle: initialize / initialized / shutdown / exit       │
│   – dispatches textDocument/* and workspace/* requests                  │
└─────────────────────────────────────────────────────────────────────────┘
                                    ▲
                                    │
┌───────────────────────────────────┴─────────────────────────────────────┐
│                      NslServer   (language logic, stateless API)        │
│   – hover(file, pos) → Hover                                            │
│   – completions(file, pos) → CompletionList                             │
│   – definition(file, pos) → Location                                    │
│   – references(file, pos) → Locations                                   │
│   – diagnostics(file)     → Diagnostic[]                                │
│   – formatRange(file, range) → TextEdit[]                               │
│   – semanticTokens(file)  → SemanticToken[]                             │
└─────────────────────────────────────────────────────────────────────────┘
                                    ▲
                                    │
┌───────────────────────────────────┴─────────────────────────────────────┐
│                 TUScheduler   (threading / caching)                     │
│   – one "translation unit" per open file                                │
│   – background threads for parse / sema                                 │
│   – serializes writes per file, parallelizes reads across files         │
│   – caches ASTs keyed by file version                                   │
└─────────────────────────────────────────────────────────────────────────┘
                                    ▲
                                    │
                          libNSLFrontend.a
```

### 3.2 LSP Features by Implementation Difficulty

| Feature | LSP method | Difficulty | Uses |
|---|---|---|---|
| Diagnostics (errors / warnings) | `publishDiagnostics` | Trivial | Sema output |
| Syntax highlighting (semantic tokens) | `textDocument/semanticTokens` | Low | Token lattice + symbol kinds |
| Hover | `textDocument/hover` | Low | Symbol lookup at position |
| Go to definition | `textDocument/definition` | Low | Symbol table `decl` back-refs |
| Find references | `textDocument/references` | Medium | Requires building a reverse-index pass across all open files |
| Auto-complete | `textDocument/completion` | Medium | Symbol table + keyword list filtered by context |
| Document symbols (outline) | `textDocument/documentSymbol` | Low | Walk top-level AST; emit `module`, `declare`, `func`, `proc`, `state` |
| Signature help | `textDocument/signatureHelp` | Low | For `func_in`/`func_out` with dummy args |
| Rename symbol | `textDocument/rename` | Medium | Find references + batch edits |
| Code actions (quick fix) | `textDocument/codeAction` | Medium–High | Requires fix-it database per diagnostic |
| Formatting | `textDocument/formatting` | Low | Delegates to `nsl-fmt` library |
| Range formatting | `textDocument/rangeFormatting` | Low | Same |
| Folding ranges | `textDocument/foldingRange` | Trivial | Walk AST for block openers |
| Inlay hints | `textDocument/inlayHint` | Medium | Show inferred bit widths next to anonymous expressions |
| Call hierarchy | `textDocument/prepareCallHierarchy` | Medium | Build proc/func call graph |

### 3.3 Incremental Document Management

Per clangd's TUScheduler pattern: TUScheduler is responsible for keeping track of the latest version of each file, building and caching ASTs and preambles as inputs, and providing threads to run requests on in an appropriate sequence.

```cpp
class NslTU {                           // one per open file
public:
    struct State {
        int                          version;     // LSP document version
        std::string                  contents;
        std::unique_ptr<CompilationUnit> ast;
        std::vector<Diagnostic>      diagnostics;
        std::unique_ptr<SymbolTable> symbols;
    };

    // Request APIs run on a worker thread; callback invoked on main thread.
    void reparse(int version, std::string contents);
    void withAST(std::function<void(const State&)> fn);

private:
    std::mutex                        mtx;
    std::condition_variable           cv;
    std::optional<State>              current;
    std::thread                       worker;
    std::atomic<bool>                 dirty = false;
};

class TUScheduler {
    llvm::StringMap<std::unique_ptr<NslTU>> tus;    // uri → TU
    ThreadPool  pool;
};
```

### 3.4 Class Diagram

```
                          ┌──────────────────────┐
                          │    NslLSPServer      │
                          │──────────────────────│
                          │ +run()               │
                          │ -dispatch(Request)   │
                          │ -onDidOpen()         │
                          │ -onDidChange()       │
                          │ -onDidClose()        │
                          └──────────┬───────────┘
                                     │ owns
            ┌────────────────────────┼────────────────────────┐
            ▼                        ▼                        ▼
 ┌────────────────────┐  ┌────────────────────┐  ┌────────────────────┐
 │    NslServer       │  │  JSONTransport     │  │  ClientCaps        │
 │────────────────────│  │────────────────────│  │────────────────────│
 │ +hover(uri,pos)    │  │ +read() Request    │  │ utf16 / utf8       │
 │ +completion(…)     │  │ +write(Response)   │  │ snippet support    │
 │ +definition(…)     │  └────────────────────┘  │ semanticTokens     │
 │ +references(…)     │                          └────────────────────┘
 │ +diagnostics(uri)  │
 │ +formatRange(…)    │
 │ +semanticTokens(…) │
 │ +documentSymbol(…) │
 └─────────┬──────────┘
           │ uses
           ▼
  ┌────────────────────┐
  │    TUScheduler     │
  │────────────────────│
  │ +open(uri)         │
  │ +update(uri, txt)  │
  │ +close(uri)        │
  │ +withAST(uri, fn)  │
  └─────────┬──────────┘
            │ manages many
            ▼
  ┌────────────────────┐
  │      NslTU         │
  │────────────────────│
  │ +reparse()         │
  │ +withAST(fn)       │
  └─────────┬──────────┘
            │ holds
            ▼
  ┌────────────────────┐
  │ libNSLFrontend     │    (shared with compiler)
  │────────────────────│
  │ Lexer              │
  │ Parser             │
  │ Sema               │
  │ SymbolTable        │
  │ Diagnostic         │
  └────────────────────┘
```

### 3.5 Example: `textDocument/hover` Flow

```cpp
// NslLSPServer::onHover
void NslLSPServer::onHover(HoverParams p, Callback<Hover> cb) {
    scheduler.withAST(p.textDocument.uri,
        [pos = p.position, cb = std::move(cb)](const NslTU::State &s) {
            auto off = lineColumnToOffset(s.contents, pos.line, pos.character);
            const Symbol *sym = s.symbols->findAtOffset(off);
            if (!sym) { cb(Hover{}); return; }

            std::string md;
            switch (sym->kind) {
              case SymbolKind::Register:
                md = "```nsl\nreg " + sym->name.str();
                if (sym->width > 1) md += "[" + std::to_string(sym->width) + "]";
                if (sym->initValue) md += " = " + *sym->initValue;
                md += ";\n```\n" + symbolDoc(sym);
                break;
              case SymbolKind::FuncIn:
                md = formatFuncSig("func_in", sym);
                break;
              // ...
            }
            cb(Hover{MarkupContent{MarkupKind::Markdown, std::move(md)},
                     sym->nameRange});
        });
}
```

---

## 4. Syntax Highlighter

Strategy: **two-tier highlighting** following the pattern VSCode uses for C++, where a fast regex layer handles keywords and literals and a smarter layer refines types and identifiers. Basic tokens are colored using a simplified TextMate grammar, and only tricky things like types are colored using setDecorations.

For NSL we do the same, using two complementary grammars:

1. **TextMate grammar** (`nsl.tmLanguage.json`) — fast, offline, works in any TextMate-compatible editor (VSCode, Sublime, Atom, TextMate itself, GitHub web). Covers keywords, literals, comments, operators, strings.
2. **Tree-sitter grammar** (`tree-sitter-nsl`) — context-aware, semantic. Distinguishes `register` vs `wire` vs `proc_name` vs `func_in` identifier references, handles `%IDENT%` preprocessor expansion sites, tags control-terminal names used in expression position (per semantic constraint S27).

The LSP's `semanticTokens` response is a third layer — the most accurate, since it has the full symbol table — and overrides the other two where available.

### 4.1 Token Categories

```
# Keywords
keyword.declaration        declare module struct
keyword.control.block      alt any if else seq for while
keyword.control.flow       goto return finish
keyword.modifier           interface simulation
keyword.directive          #include #define #undef #ifdef #ifndef #if #else #endif

# Storage classes
storage.type.register      reg
storage.type.wire          wire
storage.type.memory        mem
storage.type.integer       integer variable
storage.type.param         param_int param_str parameter
storage.type.control       func func_in func_out func_self proc proc_name state state_name first_state label_name

# Built-in names
support.type.clock         m_clock p_reset
support.function.system    _display _monitor _write _finish _stop _readmemh _readmemb _init _delay
support.variable.system    _random _time

# Literals
constant.numeric.binary    0b1010  / 4'b1010
constant.numeric.octal     0o17    / 3'o5
constant.numeric.decimal   42      / 8'd20
constant.numeric.hex       0xDEAD  / 8'hFF  (incl. Z/X/U markers)
constant.other.string      "..."

# Operators
keyword.operator.arithmetic   + - * ++ --
keyword.operator.bitwise      & | ^ ~
keyword.operator.shift        << >>
keyword.operator.comparison   == != < <= > >=
keyword.operator.logical      && || !
keyword.operator.assignment   = :=
keyword.operator.extension    # '         (N#sig, N'(sig))
keyword.operator.repeat       {  }

# Comments
comment.line.double-slash   // ...
comment.block               /* ... */

# Identifiers (refined by tree-sitter / LSP; TextMate leaves them un-scoped)
entity.name.module
entity.name.function.func
entity.name.function.proc
entity.name.function.state
variable.other.register
variable.other.wire
variable.other.memory
variable.parameter
variable.other.control      (func_in / func_out / func_self)
```

### 4.2 TextMate Grammar Skeleton

```json
{
  "name": "NSL",
  "scopeName": "source.nsl",
  "fileTypes": ["nsl", "nslh", "inc"],
  "patterns": [
    { "include": "#comments" },
    { "include": "#preprocessor" },
    { "include": "#keywords" },
    { "include": "#types" },
    { "include": "#system-names" },
    { "include": "#numbers" },
    { "include": "#strings" },
    { "include": "#operators" }
  ],
  "repository": {
    "comments": {
      "patterns": [
        { "name": "comment.line.double-slash.nsl",
          "match": "//.*$" },
        { "name": "comment.block.nsl",
          "begin": "/\\*", "end": "\\*/" }
      ]
    },
    "preprocessor": {
      "patterns": [
        { "name": "keyword.directive.nsl",
          "match": "^\\s*#(include|define|undef|ifdef|ifndef|if|else|endif)\\b" },
        { "name": "variable.other.macro.nsl",
          "match": "%[A-Za-z_][A-Za-z0-9_]*%" }
      ]
    },
    "keywords": {
      "patterns": [
        { "name": "keyword.declaration.nsl",
          "match": "\\b(declare|module|struct)\\b" },
        { "name": "keyword.control.block.nsl",
          "match": "\\b(alt|any|if|else|seq|for|while)\\b" },
        { "name": "keyword.control.flow.nsl",
          "match": "\\b(goto|return|finish)\\b" },
        { "name": "keyword.modifier.nsl",
          "match": "\\b(interface|simulation)\\b" }
      ]
    },
    "types": {
      "patterns": [
        { "name": "storage.type.register.nsl",
          "match": "\\b(reg|wire|mem|integer|variable)\\b" },
        { "name": "storage.type.control.nsl",
          "match": "\\b(func|function|func_in|func_out|func_self|proc|proc_name|state|state_name|first_state|label_name)\\b" },
        { "name": "storage.type.param.nsl",
          "match": "\\b(param_int|param_str|parameter)\\b" }
      ]
    },
    "system-names": {
      "patterns": [
        { "name": "support.type.clock.nsl",
          "match": "\\b(m_clock|p_reset)\\b" },
        { "name": "support.function.system.nsl",
          "match": "\\b_(display|monitor|write|finish|stop|readmemh|readmemb|init|delay)\\b" },
        { "name": "support.variable.system.nsl",
          "match": "\\b_(random|time)\\b" }
      ]
    },
    "numbers": {
      "patterns": [
        { "name": "constant.numeric.verilog.nsl",
          "match": "\\b\\d+'[bBoOdDhH][0-9a-fA-FxXzZuU_]+" },
        { "name": "constant.numeric.hex.nsl",
          "match": "\\b0x[0-9a-fA-F_]+" },
        { "name": "constant.numeric.binary.nsl",
          "match": "\\b0b[01_]+" },
        { "name": "constant.numeric.decimal.nsl",
          "match": "\\b\\d+\\b" }
      ]
    },
    "strings": {
      "name": "string.quoted.double.nsl",
      "begin": "\"", "end": "\"",
      "patterns": [
        { "name": "constant.character.escape.nsl",
          "match": "\\\\." }
      ]
    },
    "operators": {
      "patterns": [
        { "name": "keyword.operator.assignment.nsl",
          "match": ":=|=" },
        { "name": "keyword.operator.comparison.nsl",
          "match": "==|!=|<=|>=|<|>" },
        { "name": "keyword.operator.arithmetic.nsl",
          "match": "\\+\\+|--|\\+|-|\\*" },
        { "name": "keyword.operator.bitwise.nsl",
          "match": "&|\\||\\^|~" },
        { "name": "keyword.operator.shift.nsl",
          "match": "<<|>>" }
      ]
    }
  }
}
```

### 4.3 Tree-sitter Grammar Skeleton

```javascript
// grammar.js for tree-sitter-nsl
module.exports = grammar({
  name: 'nsl',

  extras: $ => [
    /\s+/,
    $.line_comment,
    $.block_comment,
  ],

  word: $ => $.identifier,

  rules: {
    source_file: $ => repeat($._top_level_item),

    _top_level_item: $ => choice(
      $.preprocessor_directive,
      $.struct_declaration,
      $.declare_block,
      $.module_block,
      $.top_level_parameter,
    ),

    declare_block: $ => seq(
      'declare',
      optional(field('name', $.identifier)),
      optional(field('modifier', choice('interface', 'simulation'))),
      '{',
      repeat($._declare_item),
      '}',
    ),

    module_block: $ => seq(
      'module',
      field('name', $.identifier),
      '{',
      repeat($._module_item),
      '}',
    ),

    // ... one rule per grammar production, cross-referenced to nsl_final.ebnf ...

    identifier: $ => /[A-Za-z][A-Za-z0-9_]*/,

    macro_identifier: $ => /%[A-Za-z_][A-Za-z0-9_]*%/,

    number_literal: $ => choice(
      /\d+'[bBoOdDhH][0-9a-fA-FxXzZuU_]+/,
      /0x[0-9a-fA-F_]+/,
      /0b[01_]+/,
      /0o[0-7_]+/,
      /\d+/,
    ),

    line_comment:  $ => seq('//', /.*/),
    block_comment: $ => seq('/*', /[^*]*\*+([^\/*][^*]*\*+)*/, '/'),
  }
});
```

And the accompanying `queries/highlights.scm` — highlights — Path to a highlight query.:

```scheme
;; Keywords
["declare" "module" "struct"]     @keyword
["alt" "any" "if" "else"
 "seq" "for" "while"]             @keyword.control
["goto" "return" "finish"]        @keyword.control.flow
["interface" "simulation"]        @keyword.modifier

;; Storage types
["reg" "wire" "mem" "integer" "variable"]                           @type.builtin
["func" "function" "func_in" "func_out" "func_self"
 "proc" "proc_name" "state" "state_name" "first_state"]             @keyword.storage
["param_int" "param_str" "parameter"]                               @keyword.storage

;; Semantic coloring for identifier contexts
(module_block   name: (identifier) @type)
(declare_block  name: (identifier) @type)
(register_declaration  name: (identifier) @variable)
(wire_declaration      name: (identifier) @variable)
(proc_definition       name: (identifier) @function)
(state_definition      name: (identifier) @label)

(control_call callee: (identifier) @function.call)

;; Macro expansion sites
(macro_identifier) @constant.macro

;; Numbers, strings, comments
(number_literal) @number
(string_literal) @string
(line_comment)   @comment
(block_comment)  @comment
```

### 4.4 Editor Integration Matrix

| Editor | TextMate (fast basic) | Tree-sitter (semantic) | LSP (best) |
|---|---|---|---|
| VS Code | ✅ via `contributes.grammars` | ✅ via extension using wasm tree-sitter | ✅ via `nsl-lsp` |
| Sublime Text 4 | ✅ native `.sublime-syntax` export | 🟡 needs LSP + `LSP-nsl` package | ✅ |
| Neovim | ✅ `runtime/syntax/nsl.vim` | ✅ native tree-sitter | ✅ `nvim-lspconfig` |
| Emacs | ✅ `nsl-mode.el` | ✅ via `tree-sitter.el` | ✅ `lsp-mode` / `eglot` |
| GitHub | 🟡 `linguist` PR (deferred — see `README.md` §Tooling-track note) | ❌ | ❌ |
| JetBrains | 🟡 custom language plugin | ❌ | ✅ via LSP plugin |

Priority: ship TextMate first (one JSON file), then tree-sitter (one JS grammar + queries), LSP continuously.

---

## 5. Code Formatter — `nsl-fmt`

### 5.1 Design Decision: Opinionated, Configurable

Like `gofmt`/`rustfmt`/`black`, we provide one canonical style with a small number of toggles:

```toml
# .nsl-fmt.toml
indent = 4                    # or "tab"
max_line_length = 100
spaces_around_binary_ops = true
spaces_inside_braces = false
align_struct_members = true
align_case_arrows = true      # for alt/any blocks
brace_style = "k&r"           # "k&r" | "allman"
trailing_commas = "preserve"  # "preserve" | "add" | "remove"
blank_lines_between_modules = 2
preserve_comments = "all"     # "all" | "leading_only" | "none"
```

### 5.2 Architecture

The formatter pipeline follows a directive-aware, parse-then-render
flow. Per `/speckit-clarify` Q1 (specs/010-t2-formatter-v0/spec.md
Session 2026-05-04 → Option A), the formatter parses **raw source
before preprocessing**: each preprocessor directive line is treated
as an opaque CST token, while NSL fragments between directives are
parsed by the existing `libNSLFrontend.a` lexer + parser pipeline
(no parallel parser implementation; Constitution Principle II
no-duplication rule preserved).

```text
     Source text (raw, pre-preprocessor)
          │
          ▼
    ┌──────────────────────┐
    │ DirectiveSplitter    │  scans line-oriented directives;
    │ (T2 — pre-pass)      │  emits Slice = Directive | NSLFragment
    └─────┬────────────────┘
          │ Slices: [Directive(opaque) | NSLFragment]+
          ▼
    For each Directive slice  ──► verbatim emit (FR-012a opaque)
    For each NSLFragment slice ──┐
                                 ▼
                         ┌──────────────┐
                         │  Lexer       │  (from libNSLFrontend.a)
                         └──────┬───────┘
                                │ Token stream + trivia
                                ▼
                         ┌──────────────┐
                         │  Parser      │  (CST mode via `CSTSink`
                         │              │   added to existing
                         │              │   include/nsl/Parse/Parser.h
                         │              │   per /speckit-analyze C1
                         │              │   remediation)
                         └──────┬───────┘
                                │ CST
                                ▼
                         ┌──────────────────────┐
                         │   LayoutPlanner      │  decides where lines break,
                         │   – "Wadler-Leijen"  │  indentation depth,
                         │   pretty-printer     │  alignment groups
                         └──────┬───────────────┘
                                │ Doc IR  (typed layout commands)
                                ▼
                         ┌──────────────────────┐
                         │   LayoutRenderer     │  ribbon fitting,
                         │                      │  respects max_line_length;
                         │                      │  emits exactly one trailing
                         │                      │  `\n` on non-empty output
                         │                      │  (R7 per Session 2026-05-05)
                         └──────┬───────────────┘
          ┌─────────────────────┘
          │
          ▼
   Formatted text
```

**Refusal-mode atomic semantics** (per Session 2026-05-05 Q1
strict refusal): if ANY NSLFragment slice fails to lex+parse,
`format_buffer` returns `Status::Refused` with the failing
slice's diagnostics; no partial output is emitted. Tolerated
pre-parse byte sequences are limited to those named in FR-012a
(directive lines + `%IDENT%` splices). BOM bytes, vendor
pragmas, and top-level system-task expressions are all parse
errors → refused.

The pretty-printer IR follows the Wadler-Leijen algebra:

```cpp
class Doc {
public:
    enum class Kind {
        Text,          // literal string
        Line,          // soft line break, becomes space if fits
        Nest,          // increase indent for contained doc
        Group,         // try to fit on one line, else break at every Line
        Concat,        // sequence of docs
        Align,         // align contained doc to current column
        Comment        // preserves a comment and its surrounding trivia
    };

    static Doc text(StringRef s);
    static Doc line();
    static Doc nest(int indent, Doc inner);
    static Doc group(Doc inner);
    static Doc concat(std::initializer_list<Doc>);
    static Doc align(Doc inner);
    static Doc comment(StringRef text, bool leading, bool trailing);
};

class LayoutPlanner {
public:
    Doc visit(CSTNode *node);
private:
    Doc visitModuleBlock(CSTNode*);
    Doc visitAltBlock(CSTNode*);
    Doc visitSeqBlock(CSTNode*);
    Doc visitIfStmt(CSTNode*);
    Doc visitExpression(CSTNode*);
    // ...
};
```

### 5.3 NSL-Specific Formatting Rules

1. **`alt`/`any` case alignment** — when enabled, the `:` separators align vertically within a block:
   ```nsl
   alt {
       state == IDLE    : reg := 0;
       state == RUNNING : reg := 1;
       state == DONE    : reg := 2;
   }
   ```

2. **Struct member alignment** — bit-width brackets align:
   ```nsl
   struct csr_t {
       mstatus  [32];
       mcause   [32];
       mtvec    [30];
       mepc     [32];
   };
   ```

3. **`proc_name` argument lists** — if any argument has a width, wrap each argument on its own line:
   ```nsl
   proc_name exec(
       pc   [32],
       inst [32],
       src1 [32],
       src2 [32]
   );
   ```

4. **Bit-slice and concat spacing** — consistent: `a[7:0]` never `a[ 7 : 0 ]`; `{a, b, c}` with spaces after commas, none inside braces.

5. **Operator spacing** — binary ops always spaced (`a + b`), unary never (`~a`, `!cond`).

6. **Preserves attached comments** — a comment on the same line as a declaration stays there; a block comment above a declaration stays above it.

### 5.4 Diff-style CLI

```
nsl-fmt [options] <file>...

  -i, --in-place       Rewrite files in place
  -c, --check          Exit non-zero if any file would change; print diff
      --stdin          Read from stdin, write to stdout
      --range LINE:LINE  Format only given line range (for LSP)
      --config PATH    Path to .nsl-fmt.toml
```

Integration: the LSP's `textDocument/formatting` and `rangeFormatting` handlers are thin wrappers that call into `libNslFmt.a`.

---

## 6. Linter — `nsl-lint`

### 6.1 Scope — Three Rule Tiers

1. **Grammar-level warnings** (already know from Sema, elevated as lint):
   - W001: unreachable `state` after `goto`
   - W002: unused `reg`/`wire`/`mem` declaration
   - W003: `function` keyword used (prefer `func`; S26)
   - W004: `label` used as identifier (reserved but undocumented; N10)
   - W005: shadowed identifier

2. **Semantic / style rules**:
   - S001: bit-width mismatch in assignment that compiler auto-widens
   - S002: `alt` block with only one case (use `if` instead)
   - S003: `any` block where `alt` was meant (no overlapping conditions)
   - S004: state machine with unreachable state
   - S005: `proc` never invoked anywhere in the compilation unit
   - S006: `func_in` dummy-arg shadowing a module-level signal
   - S007: comparison of signals with differing bit widths
   - S008: `goto` across multiple `seq` blocks (suspicious; check S25)
   - S009: missing `else` in `alt`/`any` when coverage is non-exhaustive
   - S010: `reg` initial value truncated (does not fit declared width)

3. **Hardware-design rules**:
   - H001: missing reset path for a `reg` that is read in the first cycle
   - H002: combinational loop detected through `wire` chain
   - H003: multi-driver on an output (two `func` bodies assign the same port)
   - H004: unregistered output that could glitch
   - H005: state machine with no transitions out of a state (deadlock)
   - H006: suspiciously large memory depth (> 4096) that might not infer to BRAM
   - H007: `mem` used as both combinational-read and sequential-write in same clock (read-during-write hazard)
   - H008: async reset without synchronizer on data path
   - H009: `func_in` called from both a combinational context and a `seq` block

### 6.2 Architecture

```
┌──────────────────────────────────┐
│      LintDriver                  │
│   1. parse with libNSLFrontend   │
│   2. build SymbolTable + AST     │
│   3. run rule set                │
│   4. emit Diagnostic[] (JSON     │
│      or human-readable)          │
└──────────┬───────────────────────┘
           │
           ▼
┌──────────────────────────────────┐
│      LintRuleRegistry            │
│   – rules registered statically  │
│   – each rule declares its       │
│     AST hook + severity          │
└──────────┬───────────────────────┘
           │
           ▼
┌──────────────────────────────────┐
│      Rule (ABC)                  │
│   +id() -> StringRef             │
│   +severity() -> Severity        │
│   +hookKind() -> HookKind        │
│   +check(node, ctx)              │
│   +suggestFix(node) -> Edit?     │
└──────────────────────────────────┘
           ▲
           │ inherits
     ┌─────┴────────────────────┐
     │                          │
┌────┴────────┐   ┌─────────────┴───────┐   ┌────────────────────┐
│ UnusedReg   │   │ MultiDriver         │   │ CombLoop           │
│ Rule        │   │ Rule                │   │ Rule               │
└─────────────┘   └─────────────────────┘   └────────────────────┘
```

### 6.3 Rule Interface

```cpp
class Rule {
public:
    virtual ~Rule() = default;
    virtual StringRef  id()       const = 0;       // e.g. "W002"
    virtual StringRef  name()     const = 0;
    virtual Severity   severity() const = 0;       // warning / error / info
    virtual HookKind   hookKind() const = 0;       // Module / Proc / Expr / ...

    // Core visitor hook. ctx gives access to symbol table, CFG, etc.
    virtual void check(Node *node, LintContext &ctx) = 0;

    // Optional: propose a fix-it edit the LSP can apply.
    virtual std::optional<Edit> suggestFix(Node*, LintContext&) { return std::nullopt; }
};

class LintContext {
public:
    const SymbolTable       &symbols;
    const ControlFlowGraph  &cfg;        // per-proc CFG built on demand
    const DataDependenceGraph &dfg;
    DiagnosticEngine        &diag;

    void report(Node *node, Severity sev, llvm::Twine msg);
    void reportWithFix(Node *node, Severity sev, llvm::Twine msg, Edit fix);
};

class LintRuleRegistry {
public:
    static LintRuleRegistry &instance();
    void registerRule(std::unique_ptr<Rule>);
    std::vector<Rule*> allRules();
    std::vector<Rule*> enabledRules(const LintConfig&);
};

#define REGISTER_RULE(RuleClass) \
    namespace { struct RuleClass##Registrar { RuleClass##Registrar() { \
        LintRuleRegistry::instance().registerRule( \
            std::make_unique<RuleClass>()); }}; \
      static RuleClass##Registrar rule_##RuleClass; }
```

### 6.4 Example Rule Implementation

```cpp
class UnusedRegRule : public Rule {
public:
    StringRef id() const override { return "W002"; }
    StringRef name() const override { return "unused-register"; }
    Severity severity() const override { return Severity::Warning; }
    HookKind hookKind() const override { return HookKind::Module; }

    void check(Node *node, LintContext &ctx) override {
        auto *mod = cast<ModuleDecl>(node);
        for (auto *reg : mod->registers()) {
            if (ctx.symbols.usageCount(reg) == 0) {
                ctx.reportWithFix(
                    reg, Severity::Warning,
                    "register '" + reg->name + "' is declared but never used",
                    Edit::removeRange(reg->sourceRange));
            }
        }
    }
};
REGISTER_RULE(UnusedRegRule)
```

### 6.5 Configuration

```toml
# .nsl-lint.toml
[severity]
W002 = "warning"       # unused reg
S002 = "info"          # single-case alt
H001 = "error"         # missing reset
H003 = "error"         # multi-driver

[disable]
rules = ["W004"]        # allow `label` identifier

[include]
paths = ["src/**/*.nsl", "include/**/*.h"]

[exclude]
paths = ["third_party/**"]
```

### 6.6 CI Integration

```
nsl-lint --format=json src/ > lint-report.json
nsl-lint --format=github src/    # emits GitHub Actions annotations
nsl-lint --fix src/              # apply fix-its in place (only safe fixes)
```

---

## 7. Cross-Tool Integration — The LSP is the Hub

All the heavy lifting lives in shared libraries. The LSP composes them:

- Format request → `libNslFmt.a`
- Document diagnostics → Sema from `libNSLFrontend.a` + lint rules from `libNslLint.a`
- Semantic tokens → token lattice from `libNSLFrontend.a` + symbol kinds
- Code actions → lint rule fix-its + standard refactors

```
┌────────────────────────────────────────────────────────────────────┐
│                         nsl-lsp binary                             │
│                                                                    │
│   ┌──────────────┐  ┌──────────────┐  ┌──────────────┐             │
│   │libNSLFrontend│  │  libNslFmt   │  │  libNslLint  │             │
│   └──────────────┘  └──────────────┘  └──────────────┘             │
│                                                                    │
└────────────────────────────────────────────────────────────────────┘
```

---

## 8. Shared Directory Layout

```
nsl-tools/
├── cmake/
├── lib/
│   ├── Frontend/               # libNSLFrontend — lexer/parser/sema/symtab
│   ├── CST/                    # concrete-syntax tree for formatter/LSP
│   ├── Fmt/                    # libNslFmt — pretty printer
│   ├── Lint/                   # libNslLint — rule registry + built-in rules
│   │   ├── Rules/
│   │   │   ├── UnusedReg.cpp
│   │   │   ├── MultiDriver.cpp
│   │   │   ├── CombLoop.cpp
│   │   │   └── ...
│   │   └── LintDriver.cpp
│   ├── LSP/                    # libNslLSP — protocol + server
│   │   ├── Protocol.h
│   │   ├── Transport.cpp
│   │   ├── Scheduler.cpp
│   │   ├── Server.cpp
│   │   └── Features/
│   │       ├── Hover.cpp
│   │       ├── Completion.cpp
│   │       ├── Definition.cpp
│   │       ├── References.cpp
│   │       ├── SemanticTokens.cpp
│   │       ├── DocumentSymbol.cpp
│   │       └── Formatting.cpp
│   └── Analysis/
│       ├── ControlFlowGraph.cpp
│       └── DataDependenceGraph.cpp
├── tools/
│   ├── nsl-lsp/                # binary entry point
│   ├── nsl-fmt/
│   └── nsl-lint/
├── grammars/
│   ├── textmate/
│   │   └── nsl.tmLanguage.json
│   └── tree-sitter-nsl/
│       ├── grammar.js
│       ├── src/parser.c        # generated
│       ├── queries/
│       │   ├── highlights.scm
│       │   ├── locals.scm
│       │   ├── indents.scm
│       │   └── folds.scm
│       └── bindings/
├── vscode-nsl/                 # VS Code extension
│   ├── package.json
│   ├── syntaxes/nsl.tmLanguage.json    # symlink to grammars/textmate/
│   ├── language-configuration.json
│   ├── src/
│   │   └── extension.ts        # thin wrapper: spawns nsl-lsp, activates TS parser
│   └── README.md
├── test/
│   ├── LSP/                    # lit-driven LSP smoke tests
│   ├── Fmt/                    # golden-file formatter tests
│   └── Lint/                   # one .nsl + .expected per rule
└── CMakeLists.txt
```

---

## 9. Milestone Plan

The tooling-track milestones (`Txx`–`Tyy`) are maintained alongside
the compiler-track in the project-root milestone plan. This section
is a routing pointer; **the schedule lives in those files, not
here** — do not duplicate the table.

- [`../../README.md`](../../README.md) §Roadmap holds the canonical
  `Txx`–`Tyy` table and the compiler `Mxx`–`Myy` table. `T1` is
  parallel from project start; the remainder of the T-track unlocks
  at compiler M3.
- [`../../CLAUDE.md`](../../CLAUDE.md) §2 holds the tooling-feature →
  milestone roll-up (LSP method, lint rule, formatter capability,
  highlighter scope, editor integration) split into sub-tables
  §2.1–§2.5.
- [`../../CONTRIBUTING.md`](../../CONTRIBUTING.md) §3.8 holds the
  workflow-tier project-enablement (`P-LIN`, `P-TS`); §3.9 holds the
  amendment rules.

---

## 10. Summary — The Value Proposition

By sharing `libNSLFrontend.a` across compiler and tooling:

- **One parser to maintain** — grammar changes propagate automatically to all five tools.
- **Consistent diagnostics** — LSP, linter, and compiler agree on what's an error.
- **Reuse already validated code** — the AST that compiled rv32x_dev and turboV is the same AST the LSP walks for hover.

Net impact: an NSL user opens a `.nsl` file in VS Code and gets:

- Syntax highlighting (TextMate fast path + tree-sitter semantic overlay)
- Inline errors and warnings (from Sema + lint rules)
- Hover showing bit widths, reg vs wire, function signatures
- Go-to-definition for every identifier including `proc_name`, `state_name`, macros
- Autocomplete for `func_in` calls with the right argument count
- Save-on-format that produces byte-identical output whether it ran on their machine, a CI runner, or through `textDocument/formatting` via the LSP

— all from the same C++ codebase that produces Verilog for their FPGA.
