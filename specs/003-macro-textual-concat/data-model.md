<!-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception -->

# Data Model: Bare-Macro Textual Concatenation

**Branch**: `003-macro-textual-concat` | **Date**: 2026-04-27
**Plan**: [plan.md](./plan.md)

The Key Entities section of [spec.md](./spec.md) lists 2 lightweight
entities (substitution context, depth counter). This file expresses
them structurally — the new `MacroExpander` class, the small
extensions to `MacroTable` (none needed, actually — read-only
consumer), and the pp.ebnf P10 amendment text.

## Entity 1 — `MacroExpander` (private, `lib/Preprocess/MacroExpander.h`)

**Purpose**: textual-substitution + cycle-detection pre-pass for
bare-identifier macro references inside `#define` body / `#if`
condition expression text.

**Representation** (private header):

```cpp
#ifndef NSL_LIB_PREPROCESS_MACROEXPANDER_H
#define NSL_LIB_PREPROCESS_MACROEXPANDER_H

#include "nsl/Basic/Diagnostic.h"
#include "nsl/Basic/SourceLocation.h"
#include "nsl/Preprocess/MacroTable.h"

#include <string>

#include "llvm/ADT/StringRef.h"

namespace nsl::preprocess {

/// Textual-substitution pre-pass for bare-identifier macro
/// references inside `#define` body and `#if` condition
/// expression text. Walks the input character stream, replaces
/// every defined-macro identifier with its body text, and
/// returns the fully-substituted result. Recursive references
/// re-trigger lookup; cycles bounded at kMaxExpansionDepth (256).
class MacroExpander {
public:
  /// Recursion budget. Matches `Preprocessor::kMaxIncludeDepth`
  /// from M1 (FR-022) for consistency.
  static constexpr unsigned kMaxExpansionDepth = 256;

  MacroExpander(MacroTable &macros, DiagnosticEngine &diag);

  /// Expand bare-identifier macro references in `text`. Returns
  /// the substituted text. On cycle detection, emits the FR-007
  /// locked diagnostic `recursive macro expansion: <NAME>` at
  /// `use_loc` and returns `text` unchanged (failsoft — the
  /// downstream parser will then surface a parse error on the
  /// unexpanded form, but the expander does not abort the
  /// expression-evaluation path).
  std::string expand(llvm::StringRef text, SourceRange use_loc);

private:
  std::string expandImpl(llvm::StringRef text, SourceRange use_loc,
                         unsigned depth);

  MacroTable &macros_;
  DiagnosticEngine &diag_;
};

} // namespace nsl::preprocess

#endif // NSL_LIB_PREPROCESS_MACROEXPANDER_H
```

**Invariants**:

- `expand("",  loc)` returns `""` (empty input → empty output).
- `expand` is a pure function of `(text, macros_, diag_)` per
  FR-016. No env-var influence; no hash-derived ordering of macro
  iteration (the table is read-only here, lookup-only).
- Recursion depth is bounded by `kMaxExpansionDepth = 256`. A
  257-level chain triggers cycle detection.
- A defined macro's body is substituted **textually** at the
  identifier's character span. Adjacent characters are NOT
  separated by inserted whitespace.
- An undefined identifier is left as-is; no diagnostic raised at
  expansion time (the expression parser raises an "unknown
  identifier" diagnostic later, per FR-017).
- String-literal content (anything between matched `"..."`) is NOT
  scanned for identifier substitution. Identifier scanning resumes
  at the closing `"`.

**Lifecycle**: created on demand by `PPExpression::parse()` and by
`IdentSplicer::splice()`; lives only for the duration of one
expression evaluation. Cheap to construct.

## Entity 2 — `MacroTable` (no changes)

The M1 `MacroTable` remains read-only from the expander's
perspective. The expander uses `lookup()` and `defined()` only;
the existing `insert()`/`redefine()`/`undef()` mutators stay
the responsibility of `DirectiveParser` and `Preprocessor`.

**Read-only API used by `MacroExpander`**:

```cpp
const MacroDef *MacroTable::lookup(llvm::StringRef name) const;
bool            MacroTable::defined(llvm::StringRef name) const;
```

**No structural change**. No new public API. The expander is the
new caller; the table is unchanged.

## Entity 3 — `pp.ebnf` P10 amendment text

The exact spec amendment per FR-001 / FR-002. **Same line count as
the existing P10** (5 lines for the bullet, ±0 lines net) so the
file's 559-line cap holds (SC-006).

**Existing P10 text** (pp.ebnf lines 491–500):

```
 *  (P10) ORDERING OF EXPANSION INSIDE #define.
 *        Within a  #define  replacement body, the order of operations is:
 *          1.  Recognize  %IDENT%  splices, look up the inner macro, and
 *              splice in the replacement TEXT (not the value — splicing
 *              is textual).
 *          2.  Evaluate compile-time helper calls left-to-right.
 *          3.  Reduce the resulting expression with the operator
 *              precedence given in §3.
 *        The final value (integer or real) becomes the macro's value for
 *        future expansions and  #if  comparisons.
```

**Amended P10 text** (verbatim from `docs/spec/nsl_pp.ebnf:491–500` post-amendment):

```
 *  (P10) ORDERING OF EXPANSION INSIDE #define / #if.
 *        Within a  #define  replacement body or  #if  condition:
 *          1.  TEXTUAL SUBSTITUTION: replace bare-identifier macro
 *              references AND  %IDENT%  splices with the macro's
 *              body TEXT (not its value); substituted text adjoins
 *              surrounding characters without inserted whitespace;
 *              result is re-tokenized.  Recursion bounded at 256.
 *          2.  Evaluate compile-time helper calls left-to-right.
 *          3.  Reduce per §3 operator-precedence.
 *        Final value becomes the macro's value for future use.
```

The amendment makes explicit that **bare identifiers AND `%IDENT%`
forms BOTH undergo textual substitution** in step 1, replacing the
former implicit "%IDENT%-only" reading. Adjacency rules are stated
("adjoins surrounding characters without inserted whitespace");
recursion bound stated ("256"). The reduction step (3) is
slightly tightened to reference §3 explicitly.

**Line count** (verified post-edit): the P10 paragraph itself is
10 lines of body inside the comment block; the file `nsl_pp.ebnf`
remains at exactly **559 lines**, identical to the pre-amendment
total → **net 0** change. SC-006's "±2 line" budget is respected
with margin to spare.

## Validation checklist for the design

Per spec FR roll-up:
- **FR-001** (pp.ebnf P10 amendment): entity 3 above is the exact text.
- **FR-002** (pp.ebnf §3 `pp_primary_expr` unchanged): entity 1's expander runs BEFORE the parser sees the input — the parser's primary-expression rule is unchanged.
- **FR-003** (pre-pass before tokenization): entity 1's `expand()` runs first; the parser receives the already-substituted character buffer.
- **FR-004** (no whitespace insertion): entity 1's invariants explicitly state "adjacent characters NOT separated by inserted whitespace."
- **FR-005** (recursive expansion): entity 1's `expandImpl()` recursion + the kMaxExpansionDepth budget.
- **FR-006** (256-level cycle bound): entity 1's `kMaxExpansionDepth = 256`.
- **FR-007** (locked diagnostic): entity 1's `expand()` emits `recursive macro expansion: <NAME>` at cycle detection.
- **FR-008** (substitution before helper eval): entity 3's amended P10 step 1 explicitly says textual substitution happens BEFORE helper evaluation in step 2.
- **FR-009** (passthrough lines unaffected): entity 1's `expand()` is only called from `PPExpression::parse()` (used for `#if` conditions and `#define` body expressions). `IdentSplicer` already had the right scope; the expander is consulted from the same scope.
- **FR-010..FR-013** (test gates): tasks.md will sequence the 3 lit fixtures + 1 gtest unit suite.
- **FR-014** (optional revert of M1 workarounds): tasks.md task list includes the reverts as optional clean-up tasks.
- **FR-015** (test-first per Principle VIII): tasks.md sequences fixtures BEFORE implementation.
- **FR-016** (determinism): entity 1's invariants state "pure function of (text, macros_, diag_)."
- **FR-017** (undefined identifier): entity 1's invariants state "left as-is; no diagnostic at expansion time."

If any cell of the FR roll-up has no corresponding entity here, that is a design gap to fix before tasks.md is generated. Currently: all 17 FRs covered.
