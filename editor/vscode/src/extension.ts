// Kryga editor extension — Phase 1 frame consumer.
//
// Opens a webview viewport, attaches to the engine's shared-memory region,
// and pumps frames into the webview at 60 Hz (polling).

import * as vscode from "vscode";

import { loadNative } from "./native";
import { ViewportSession } from "./viewport";
import { InspectorSession } from "./inspector";

export function activate(context: vscode.ExtensionContext): void {
  const viewports: ViewportSession[] = [];
  const inspectors: InspectorSession[] = [];

  async function promptConfig(): Promise<
    { name: string; maxWidth: number; maxHeight: number } | null
  > {
    const config = vscode.workspace.getConfiguration("kryga");
    const defaultName = config.get<string>("ipcName") ?? "default";
    const name = await vscode.window.showInputBox({
      prompt: "IPC channel name (passed to engine as --editor-ipc <name>)",
      value: defaultName,
    });
    if (!name) return null;
    return {
      name,
      maxWidth: config.get<number>("maxWidth") ?? 1024,
      maxHeight: config.get<number>("maxHeight") ?? 1024,
    };
  }

  const openCmd = vscode.commands.registerCommand(
    "kryga.openViewport",
    async () => {
      const cfg = await promptConfig();
      if (!cfg) return;

      let native;
      try {
        native = loadNative();
      } catch (e) {
        vscode.window.showErrorMessage(
          `Kryga: native addon is not built yet. ${(e as Error).message}`,
        );
        return;
      }

      viewports.push(new ViewportSession(native, cfg, context.extensionUri));
    },
  );

  const openInspectorCmd = vscode.commands.registerCommand(
    "kryga.openInspector",
    async () => {
      const cfg = await promptConfig();
      if (!cfg) return;

      let native;
      try {
        native = loadNative();
      } catch (e) {
        vscode.window.showErrorMessage(
          `Kryga: native addon is not built yet. ${(e as Error).message}`,
        );
        return;
      }

      inspectors.push(new InspectorSession(native, cfg, context.extensionUri));
    },
  );

  context.subscriptions.push(openCmd, openInspectorCmd);
  context.subscriptions.push({
    dispose: () => {
      for (const s of viewports) s.dispose();
      for (const s of inspectors) s.dispose();
    },
  });
}

export function deactivate(): void {}
