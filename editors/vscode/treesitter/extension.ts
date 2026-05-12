// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// editors/vscode/treesitter/extension.ts — VS Code extension entry
// point for the T8 tree-sitter-driven semantic-tokens layer.
//
// Cross-references:
//   - contracts/vscode-extension.contract.md §2 (activation surface)
//   - data-model.md §1.5 (extension-shell entity)
//   - acceptance scenarios AS3.1, AS3.3
//
// Activation flow per contract §2:
//   1. web-tree-sitter Parser.init({locateFile: …}) — emscripten
//      loader path resolution.
//   2. Language.load(<extension-dir>/tree-sitter-nsl.wasm) — async
//      WASM binary load.
//   3. Read <extension-dir>/queries/highlights.scm (mirror of the
//      canonical grammars/treesitter/queries/highlights.scm; T033
//      lands the canonical, byte-equality CI gate keeps mirror in
//      sync — same precedent as T1's nsl.tmLanguage.json materialised
//      copy at editors/vscode/syntaxes/).
//   4. Construct + register HighlightProvider for the `nsl` language
//      ID via vscode.languages.registerDocumentSemanticTokensProvider.
//   5. Push subscription onto ctx.subscriptions for clean teardown
//      on extension deactivate.
//
// Graceful degradation (AS3.3): when tree-sitter-nsl.wasm OR
// queries/highlights.scm is missing from the extension folder,
// surface a vscode.window.showWarningMessage and exit activate()
// cleanly without throwing. The user keeps T1's TextMate
// colouring; T8's overrides do not apply.

import * as fs from 'fs';
import * as path from 'path';
import * as vscode from 'vscode';

import Parser = require('web-tree-sitter');

import { LEGEND } from './legend';
import { HighlightProvider } from './highlight-provider';

const NSL_LANGUAGE_ID = 'nsl';
const NSL_DOC_SELECTOR: vscode.DocumentSelector = { language: NSL_LANGUAGE_ID };

const WASM_BASENAME = 'tree-sitter-nsl.wasm';
const QUERY_REL_PATH = path.join('queries', 'highlights.scm');

export async function activate(context: vscode.ExtensionContext): Promise<void> {
    const wasmPath = path.join(context.extensionPath, 'treesitter', WASM_BASENAME);
    const queryPath = path.join(context.extensionPath, 'treesitter', QUERY_REL_PATH);

    if (!fs.existsSync(wasmPath)) {
        void vscode.window.showWarningMessage(
            `NSL: tree-sitter grammar (${WASM_BASENAME}) not found in extension folder. ` +
                `Download it from the latest CI workflow artefact (tree-sitter-nsl-wasm) or ` +
                `from the corresponding GitHub Release, and place it under ` +
                `treesitter/ inside the extension. T8 semantic-tokens disabled; T1 ` +
                `TextMate colouring still applies.`,
        );
        return;
    }

    if (!fs.existsSync(queryPath)) {
        void vscode.window.showWarningMessage(
            `NSL: highlight query file (${QUERY_REL_PATH}) not found in extension ` +
                `folder. T8 semantic-tokens disabled; T1 TextMate colouring still applies.`,
        );
        return;
    }

    try {
        await Parser.init({
            locateFile: (filename: string, _scriptDirectory: string) => {
                // Inside the VS Code extension host, web-tree-sitter's emscripten
                // loader looks for `tree-sitter.wasm` (the engine itself, not the
                // grammar). Serve it from the bundled node_modules path.
                if (filename === 'tree-sitter.wasm') {
                    return path.join(
                        context.extensionPath,
                        'node_modules',
                        'web-tree-sitter',
                        'tree-sitter.wasm',
                    );
                }
                return filename;
            },
        });

        const language = await Parser.Language.load(wasmPath);
        const parser = new Parser();
        parser.setLanguage(language);

        const queryText = await fs.promises.readFile(queryPath, 'utf-8');
        const query = language.query(queryText);

        const provider = new HighlightProvider(parser, query);

        context.subscriptions.push(
            vscode.languages.registerDocumentSemanticTokensProvider(
                NSL_DOC_SELECTOR,
                provider,
                LEGEND,
            ),
        );

        // Clean teardown: parser carries native (WASM) state that
        // benefits from explicit delete on extension unload.
        context.subscriptions.push(
            new vscode.Disposable(() => {
                provider.dispose();
                parser.delete();
            }),
        );
    } catch (err) {
        void vscode.window.showWarningMessage(
            `NSL: tree-sitter initialisation failed (${err instanceof Error ? err.message : String(err)}). ` +
                `T8 semantic-tokens disabled; T1 TextMate colouring still applies.`,
        );
    }
}

export function deactivate(): void {
    // No-op: per-document state is owned by HighlightProvider's
    // tree cache, disposed via context.subscriptions when the
    // extension host tears down.
}
