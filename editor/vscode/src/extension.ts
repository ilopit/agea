// Phase 0 scaffold for the Kryga VS Code editor.
//
// No IPC, no N-API addon, no frame transport yet — those land in Phase 1.
// This file exists only to keep the extension project buildable and to host a
// single command that opens a placeholder webview panel. The command body is
// deliberately trivial; any real behaviour belongs in a later phase.

import * as vscode from "vscode";

export function activate(context: vscode.ExtensionContext): void {
  const disposable = vscode.commands.registerCommand(
    "kryga.openViewport",
    () => {
      const panel = vscode.window.createWebviewPanel(
        "kryga.viewport",
        "Kryga Viewport",
        vscode.ViewColumn.Active,
        { enableScripts: true, retainContextWhenHidden: true },
      );

      panel.webview.html = `<!doctype html>
<html>
  <body style="background:#111;color:#bbb;font-family:sans-serif;padding:1rem;">
    <h2>Kryga Viewport (Phase 0 scaffold)</h2>
    <p>Frame transport is wired in Phase 1.</p>
  </body>
</html>`;
    },
  );

  context.subscriptions.push(disposable);
}

export function deactivate(): void {}
