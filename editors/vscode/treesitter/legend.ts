// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// editors/vscode/treesitter/legend.ts — single source of truth for
// the 21-entry SemanticTokensLegend, shared between extension.ts (which
// passes it to vscode.languages.registerDocumentSemanticTokensProvider)
// and highlight-provider.ts (which uses the indices when building the
// SemanticTokens response).
//
// The order MUST match `editors/vscode/package.json`
// `contributes.semanticTokenTypes` byte-for-byte and `data-model.md`
// §1.4 (#1–#20) plus the FR-009 control-terminal #21
// (`contracts/highlights-coverage.contract.md` §2). Any reorder here
// MUST be mirrored in package.json in the same commit.

import * as vscode from 'vscode';

export const TOKEN_TYPES: readonly string[] = [
    'keyword',                    // #1
    'keyword.control',            // #2
    'keyword.control.flow',       // #3
    'keyword.modifier',           // #4
    'keyword.storage',            // #5
    'type.builtin',               // #6
    'type',                       // #7
    'function.call',              // #8
    'constant.macro',             // #9
    'number',                     // #10
    'string',                     // #11
    'comment',                    // #12
    'variable.register',          // #13
    'variable.wire',              // #14
    'variable.memory',            // #15
    'function.proc',              // #16
    'function.func',              // #17
    'function.call.proc',         // #18
    'function.call.func',         // #19
    'label.state',                // #20
    'variable.builtin.terminal',  // #21 (FR-009; locked by T019)
];

export const TOKEN_MODIFIERS: readonly string[] = [];

export const LEGEND = new vscode.SemanticTokensLegend(
    [...TOKEN_TYPES],
    [...TOKEN_MODIFIERS],
);

export const TOKEN_TYPE_INDEX: ReadonlyMap<string, number> = new Map(
    TOKEN_TYPES.map((name, idx) => [name, idx]),
);
