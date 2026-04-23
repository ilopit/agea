// Kryga editor extension — Phase 1 frame consumer.
//
// Opens a webview viewport, attaches to the engine's shared-memory region,
// and pumps frames into the webview at 60 Hz (polling).

import * as vscode from "vscode";

import { loadNative } from "./native";
import { ViewportSession } from "./viewport";

export function activate(context: vscode.ExtensionContext): void {
  const sessions: ViewportSession[] = [];

  const openCmd = vscode.commands.registerCommand(
    "kryga.openViewport",
    async () => {
      const config = vscode.workspace.getConfiguration("kryga");
      const defaultName = config.get<string>("ipcName") ?? "default";
      const maxWidth = config.get<number>("maxWidth") ?? 1024;
      const maxHeight = config.get<number>("maxHeight") ?? 1024;

      const name = await vscode.window.showInputBox({
        prompt: "IPC channel name (passed to engine as --editor-ipc <name>)",
        value: defaultName,
      });
      if (!name) return;

      let native;
      try {
        native = loadNative();
      } catch (e) {
        vscode.window.showErrorMessage(
          `Kryga: native addon is not built yet. ${(e as Error).message}`,
        );
        return;
      }

      const session = new ViewportSession(
        native,
        { name, maxWidth, maxHeight },
        context.extensionUri,
      );
      sessions.push(session);
    },
  );

  context.subscriptions.push(openCmd);
  context.subscriptions.push({
    dispose: () => {
      for (const s of sessions) s.dispose();
    },
  });
}

export function deactivate(): void {}
