// Property inspector panel. Phase 4 surfaces exactly one thing — the edit-
// camera position — via drag-scrub Vector3 inputs. Phase 5 will generalise
// over the reflection system so new component types appear automatically.
//
// Message flow:
//
//   click "Refresh"   → postMessageIn("requestSelection")
//   engine            → push_message_out("selection entity=... pos_x=... pos_y=... pos_z=...")
//   poll 100 ms       → drainMessagesOut() → render selected fields
//   drag number input → postMessageIn("setProperty path=camera.position.x value=1.5")
//   engine            → push_message_out("propertyChanged path=... value=...")
//   poll              → update UI to echoed value
//
// Everything is ASCII whitespace-separated `key=value` tokens so we don't
// need to ship a JSON library on either side.

import * as vscode from "vscode";
import * as path from "path";
import * as fs from "fs";

import { KrygaNative } from "./native";
import { computeRegionBytes } from "./protocol";

const POLL_MS = 100;

interface InspectorConfig {
  name: string;
  maxWidth: number;
  maxHeight: number;
}

export class InspectorSession {
  private readonly panel: vscode.WebviewPanel;
  private handle = -1;
  private timer: NodeJS.Timeout | null = null;
  private disposed = false;

  constructor(
    private readonly native: KrygaNative,
    private readonly cfg: InspectorConfig,
    private readonly extensionUri: vscode.Uri,
  ) {
    this.panel = vscode.window.createWebviewPanel(
      "kryga.inspector",
      `Kryga Inspector: ${cfg.name}`,
      { viewColumn: vscode.ViewColumn.Beside, preserveFocus: true },
      {
        enableScripts: true,
        retainContextWhenHidden: true,
        localResourceRoots: [vscode.Uri.joinPath(extensionUri, "media")],
      },
    );

    this.panel.webview.html = this.renderHtml();
    this.panel.webview.onDidReceiveMessage((msg) => this.onWebviewMessage(msg));
    this.panel.onDidDispose(() => this.dispose());

    this.tryAttach();
  }

  private tryAttach(): void {
    if (this.disposed) return;
    const sizeBytes = computeRegionBytes(this.cfg.maxWidth, this.cfg.maxHeight);
    this.handle = this.native.open(this.cfg.name, sizeBytes);
    if (this.handle < 0) {
      setTimeout(() => this.tryAttach(), 500);
      return;
    }
    this.timer = setInterval(() => this.pump(), POLL_MS);
    this.panel.webview.postMessage({ type: "status", state: "connected" });
    // Ask for the current selection so the inspector isn't blank on open.
    this.native.postMessageIn(this.handle, "requestSelection");
  }

  private pump(): void {
    if (this.disposed || this.handle < 0) return;
    const header = this.native.readHeader(this.handle);
    if (header.publisherAlive === 0) {
      this.cleanupHandle();
      this.panel.webview.postMessage({ type: "status", state: "disconnected" });
      this.tryAttach();
      return;
    }
    const msgs = this.native.drainMessagesOut(this.handle);
    for (const m of msgs) {
      this.panel.webview.postMessage({ type: "engineMessage", payload: m });
    }
  }

  private onWebviewMessage(msg: any): void {
    if (!msg || typeof msg !== "object") return;
    if (this.handle < 0) return;
    if (msg.type === "postMessageIn" && typeof msg.payload === "string") {
      this.native.postMessageIn(this.handle, msg.payload);
    }
  }

  private cleanupHandle(): void {
    if (this.timer) {
      clearInterval(this.timer);
      this.timer = null;
    }
    if (this.handle >= 0) {
      this.native.close(this.handle);
      this.handle = -1;
    }
  }

  dispose(): void {
    if (this.disposed) return;
    this.disposed = true;
    this.cleanupHandle();
  }

  private renderHtml(): string {
    const mediaDir = vscode.Uri.joinPath(this.extensionUri, "media");
    const scriptUri = this.panel.webview.asWebviewUri(
      vscode.Uri.joinPath(mediaDir, "inspector.js"),
    );
    const htmlPath = path.join(mediaDir.fsPath, "inspector.html");
    let html = fs.readFileSync(htmlPath, "utf8");
    html = html.replace("{{INSPECTOR_JS}}", scriptUri.toString());
    html = html.replace("{{CSP_SOURCE}}", this.panel.webview.cspSource);
    return html;
  }
}
