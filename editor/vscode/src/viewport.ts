// ViewportSession — owns one webview panel, attaches to the engine's shared-
// memory region, and pumps frames into the webview.
//
// Phase 1 polled every 16ms. Phase 2 replaces the timer with the addon's
// subscribeFrames worker thread, which waits on the engine-signaled
// frame_ready event and invokes a JS callback on each signal. We still keep
// a slow (~1 Hz) watchdog timer to catch publisher_alive transitions, since
// the consumer never gets a signal for "publisher went away".

import * as vscode from "vscode";
import * as path from "path";
import * as fs from "fs";

import { KrygaNative } from "./native";
import {
  computeRegionBytes,
  InputEvent,
  InputEventType,
  SLOT_NONE,
} from "./protocol";

interface ViewportConfig {
  name: string;
  maxWidth: number;
  maxHeight: number;
}

export class ViewportSession {
  private readonly panel: vscode.WebviewPanel;
  private handle = -1;
  private lastFrameCounter = 0n;
  private watchdog: NodeJS.Timeout | null = null;
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

    // Relay input from the webview into the shm ring.
    this.panel.webview.onDidReceiveMessage((msg) => this.onWebviewMessage(msg));

    this.panel.onDidDispose(() => this.dispose());

    // Attach lazily — poll open() until the engine is up.
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

    const subscribed = this.native.subscribeFrames(this.handle, {
      name: this.cfg.name,
      callback: () => this.onFrameReady(),
    });

    if (!subscribed) {
      // Event not yet created by engine; fall back to polling the header at
      // 1 Hz until we can resubscribe. Rare — the publisher creates the
      // event before publishing any frames.
      this.watchdog = setInterval(() => this.pollOnce(), 1000);
      return;
    }

    // Slow watchdog to observe publisher_alive transitions. The worker
    // thread only fires for frame_ready signals, not for lifecycle events.
    this.watchdog = setInterval(() => this.checkAliveness(), 1000);
  }

  private onFrameReady(): void {
    if (this.disposed || this.handle < 0) return;
    this.pollOnce();
  }

  private pollOnce(): void {
    const header = this.native.readHeader(this.handle);
    if (header.publisherAlive === 0) {
      this.cleanupHandle();
      this.panel.webview.postMessage({ type: "status", state: "disconnected" });
      this.tryAttachAndStart();
      return;
    }

    if (header.frameCounter === this.lastFrameCounter) return;

    const slot = header.latestReadySlot;
    if (slot === SLOT_NONE) return;

    this.native.claimSlot(this.handle, slot);
    try {
      const buf = this.native.getSlotBuffer(this.handle, slot);
      const pixels = Buffer.from(
        buf.slice(0, header.currentWidth * header.currentHeight * 4),
      );

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

  private checkAliveness(): void {
    if (this.disposed || this.handle < 0) return;
    const header = this.native.readHeader(this.handle);
    if (header.publisherAlive === 0) {
      this.cleanupHandle();
      this.panel.webview.postMessage({ type: "status", state: "disconnected" });
      this.tryAttachAndStart();
    }
  }

  private onWebviewMessage(msg: any): void {
    if (this.disposed || this.handle < 0) return;
    if (!msg || typeof msg !== "object") return;

    switch (msg.type) {
      case "input.mouseMove": {
        const ev: InputEvent = {
          type: InputEventType.MouseMove,
          timestampMs: Date.now() & 0xffffffff,
          a: msg.x | 0,
          b: msg.y | 0,
          c: msg.dx | 0,
          d: msg.dy | 0,
        };
        this.native.postInput(this.handle, ev);
        break;
      }
      case "input.mouseButton": {
        const ev: InputEvent = {
          type: InputEventType.MouseButton,
          timestampMs: Date.now() & 0xffffffff,
          a: msg.button | 0,
          b: msg.down ? 1 : 0,
          c: 0,
          d: 0,
        };
        this.native.postInput(this.handle, ev);
        break;
      }
      case "input.mouseWheel": {
        const ev: InputEvent = {
          type: InputEventType.MouseWheel,
          timestampMs: Date.now() & 0xffffffff,
          a: Math.round((msg.deltaY ?? 0) * 120),
          b: Math.round((msg.deltaX ?? 0) * 120),
          c: 0,
          d: 0,
        };
        this.native.postInput(this.handle, ev);
        break;
      }
      case "input.key": {
        const ev: InputEvent = {
          type: InputEventType.Key,
          timestampMs: Date.now() & 0xffffffff,
          a: msg.keyCode | 0,
          b: msg.down ? 1 : 0,
          c: msg.mods | 0,
          d: 0,
        };
        this.native.postInput(this.handle, ev);
        break;
      }
      default:
        break;
    }
  }

  private cleanupHandle(): void {
    if (this.watchdog) {
      clearInterval(this.watchdog);
      this.watchdog = null;
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
