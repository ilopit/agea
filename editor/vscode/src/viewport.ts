// ViewportSession — owns one webview panel plus its poll loop.
//
// Poll loop:
//   1. readHeader → if frameCounter did not advance, skip.
//   2. claimSlot(latestReadySlot) so the publisher avoids it.
//   3. getSlotBuffer → postMessage to the webview with the pixel bytes, the
//      reported currentWidth/currentHeight, and the pixelFormat.
//   4. releaseSlot.
//
// Phase 1 is strictly polling — Phase 2 replaces step (1) with a signaled
// wait via a worker thread in the addon.
//
// The postMessage call structured-clones the Buffer once from the extension
// host into the webview. That is one more copy than zero-copy-shared-memory
// can offer; we accept it for Phase 1 and revisit if profiling shows the
// channel is the bottleneck (see editor/README.md caveats).

import * as vscode from "vscode";
import * as path from "path";
import * as fs from "fs";

import { KrygaNative } from "./native";
import { computeRegionBytes, SLOT_NONE } from "./protocol";

interface ViewportConfig {
  name: string;
  maxWidth: number;
  maxHeight: number;
}

const POLL_INTERVAL_MS = 16;

export class ViewportSession {
  private readonly panel: vscode.WebviewPanel;
  private handle = -1;
  private lastFrameCounter = 0n;
  private timer: NodeJS.Timeout | null = null;
  private disposed = false;

  constructor(
    private readonly native: KrygaNative,
    private readonly cfg: ViewportConfig,
    private readonly extensionUri: vscode.Uri,
  ) {
    this.panel = vscode.window.createWebviewPanel(
      "kryga.viewport",
      `Kryga: ${cfg.name}`,
      vscode.ViewColumn.Active,
      {
        enableScripts: true,
        retainContextWhenHidden: true,
        localResourceRoots: [vscode.Uri.joinPath(extensionUri, "media")],
      },
    );

    this.panel.webview.html = this.renderHtml();
    this.panel.onDidDispose(() => this.dispose());

    // Attach lazily — poll the addon's open() until it succeeds so the
    // extension can be started before the engine.
    this.tryAttachAndStart();
  }

  private tryAttachAndStart(): void {
    if (this.disposed) return;

    const sizeBytes = computeRegionBytes(this.cfg.maxWidth, this.cfg.maxHeight);
    this.handle = this.native.open(this.cfg.name, sizeBytes);

    if (this.handle < 0) {
      this.panel.webview.postMessage({ type: "status", state: "disconnected" });
      setTimeout(() => this.tryAttachAndStart(), 500);
      return;
    }

    this.panel.webview.postMessage({ type: "status", state: "connected" });
    this.timer = setInterval(() => this.pump(), POLL_INTERVAL_MS);
  }

  private pump(): void {
    if (this.disposed || this.handle < 0) return;

    const header = this.native.readHeader(this.handle);
    if (header.publisherAlive === 0) {
      // Publisher went away. Drop the handle and retry attach.
      this.cleanupHandle();
      this.panel.webview.postMessage({ type: "status", state: "disconnected" });
      this.tryAttachAndStart();
      return;
    }

    if (header.frameCounter === this.lastFrameCounter) {
      return;
    }

    const slot = header.latestReadySlot;
    if (slot === SLOT_NONE) {
      return;
    }

    this.native.claimSlot(this.handle, slot);
    try {
      const buf = this.native.getSlotBuffer(this.handle, slot);
      // Copy out of the mmap region before releasing the claim — once the
      // publisher is free to overwrite this slot, the Buffer contents are no
      // longer stable. Plus postMessage will structured-clone it anyway.
      const pixels = Buffer.from(buf.slice(0, header.currentWidth * header.currentHeight * 4));

      this.panel.webview.postMessage({
        type: "frame",
        width: header.currentWidth,
        height: header.currentHeight,
        pixelFormat: header.pixelFormat,
        frameCounter: header.frameCounter.toString(),
        pixels: pixels,
      });
    } finally {
      this.native.releaseSlot(this.handle);
    }

    this.lastFrameCounter = header.frameCounter;
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
    this.lastFrameCounter = 0n;
  }

  dispose(): void {
    if (this.disposed) return;
    this.disposed = true;
    this.cleanupHandle();
  }

  private renderHtml(): string {
    const mediaDir = vscode.Uri.joinPath(this.extensionUri, "media");
    const scriptUri = this.panel.webview.asWebviewUri(
      vscode.Uri.joinPath(mediaDir, "viewport.js"),
    );

    const htmlPath = path.join(mediaDir.fsPath, "viewport.html");
    let html = fs.readFileSync(htmlPath, "utf8");
    html = html.replace("{{VIEWPORT_JS}}", scriptUri.toString());
    html = html.replace("{{CSP_SOURCE}}", this.panel.webview.cspSource);
    return html;
  }
}
