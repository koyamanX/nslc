// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// editors/vscode/treesitter/highlight-provider.ts — VS Code
// DocumentSemanticTokensProvider implementation for the T8
// tree-sitter layer.
//
// Cross-references:
//   - contracts/vscode-extension.contract.md §3 (provider behaviour)
//   - data-model.md §1.5 (provider entity)
//   - acceptance scenario AS3.2 (T1 base + T8 override)
//
// Behaviour per contract §3:
//   1. Parse document.getText() with the cached Parser instance,
//      passing the previous tree if available (incremental parse).
//   2. Run the prebuilt highlights query against the parse tree.
//   3. Map each capture to a (line, char, length, tokenTypeIdx)
//      tuple via the shared LEGEND.
//   4. Emit through vscode.SemanticTokensBuilder.
//   5. Skip captures whose name is not in the legend (defensive —
//      a query referencing a capture not in TOKEN_TYPES is a
//      pairing bug between highlights.scm and legend.ts; we log
//      and continue so one stale capture doesn't suppress all
//      others).
//   6. Skip captures whose ancestor is an (ERROR) node (so a
//      malformed-source region does not crash the provider; the
//      surrounding well-parsed regions still highlight).
//   7. Honour vscode.CancellationToken — abort and return
//      partially-built tokens when cancelled.
//
// Tree-cache lifecycle:
//   - keyed by document URI string
//   - replaced on every provideDocumentSemanticTokens call
//   - the previous tree is .delete()d to release WASM memory
//   - dispose() drops every cached tree

import * as vscode from 'vscode';

// web-tree-sitter is published as a CommonJS module; the ambient
// type uses the default-export shape, but we only need the
// constructor and the static Language inner here.
import Parser = require('web-tree-sitter');

import { LEGEND, TOKEN_TYPE_INDEX } from './legend';

export class HighlightProvider implements vscode.DocumentSemanticTokensProvider {
    private readonly parser: Parser;
    private readonly query: Parser.Query;
    private readonly trees = new Map<string, Parser.Tree>();
    private readonly missingCaptureNames = new Set<string>();

    constructor(parser: Parser, query: Parser.Query) {
        this.parser = parser;
        this.query = query;
    }

    provideDocumentSemanticTokens(
        document: vscode.TextDocument,
        cancellation: vscode.CancellationToken,
    ): vscode.ProviderResult<vscode.SemanticTokens> {
        if (cancellation.isCancellationRequested) {
            return undefined;
        }

        const uri = document.uri.toString();
        const oldTree = this.trees.get(uri);
        const newTree = this.parser.parse(document.getText(), oldTree);

        if (newTree === null) {
            // Parser returned null (timeout or unrecoverable). Drop
            // any stale cache entry; T1 colouring takes over.
            if (oldTree) {
                oldTree.delete();
                this.trees.delete(uri);
            }
            return undefined;
        }

        // Replace cached tree, releasing the previous one's
        // WASM-side memory.
        if (oldTree && oldTree !== newTree) {
            oldTree.delete();
        }
        this.trees.set(uri, newTree);

        const builder = new vscode.SemanticTokensBuilder(LEGEND);
        const captures = this.query.captures(newTree.rootNode);

        for (const capture of captures) {
            if (cancellation.isCancellationRequested) {
                return undefined;
            }

            const tokenIdx = TOKEN_TYPE_INDEX.get(capture.name);
            if (tokenIdx === undefined) {
                if (!this.missingCaptureNames.has(capture.name)) {
                    this.missingCaptureNames.add(capture.name);
                    console.warn(
                        `[nsl-treesitter] capture name "${capture.name}" emitted by highlights.scm ` +
                            `is not in the SemanticTokensLegend (TOKEN_TYPES). Skipping. ` +
                            `Sync the legend with highlights.scm.`,
                    );
                }
                continue;
            }

            if (hasErrorAncestor(capture.node)) {
                continue;
            }

            this.pushNodeRange(builder, capture.node, tokenIdx);
        }

        return builder.build();
    }

    /**
     * Push a tree-sitter node's text range into the SemanticTokensBuilder,
     * splitting into per-line segments when the node spans multiple lines.
     * VS Code's semantic-token data is fundamentally per-line, so a
     * multi-line token (e.g. a block comment) MUST be emitted as N entries.
     */
    private pushNodeRange(
        builder: vscode.SemanticTokensBuilder,
        node: Parser.SyntaxNode,
        tokenIdx: number,
    ): void {
        const start = node.startPosition;
        const end = node.endPosition;
        if (start.row === end.row) {
            const length = end.column - start.column;
            if (length > 0) {
                builder.push(start.row, start.column, length, tokenIdx, 0);
            }
            return;
        }
        // Multi-line: emit one segment per line spanned.
        // Line N (start row): from start.column to end-of-line — the
        // length here is unknown without the document; over-estimate
        // to a large value and let VS Code clip to line length, which
        // it does silently per the SemanticTokensBuilder contract.
        const FAR = 10000;
        builder.push(start.row, start.column, FAR, tokenIdx, 0);
        for (let row = start.row + 1; row < end.row; row++) {
            builder.push(row, 0, FAR, tokenIdx, 0);
        }
        // Final line: 0 to end.column.
        if (end.column > 0) {
            builder.push(end.row, 0, end.column, tokenIdx, 0);
        }
    }

    dispose(): void {
        for (const tree of this.trees.values()) {
            tree.delete();
        }
        this.trees.clear();
        this.query.delete();
    }
}

function hasErrorAncestor(node: Parser.SyntaxNode): boolean {
    let current: Parser.SyntaxNode | null = node;
    while (current !== null) {
        if (current.hasError && current.type === 'ERROR') {
            return true;
        }
        current = current.parent;
    }
    return false;
}
