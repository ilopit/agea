// JSON-RPC client over a TCP socket. Uses vscode-jsonrpc for LSP-style
// Content-Length framing on the wire.
//
// Engine writes a discovery file at `<project>/tmp/editor_rpc.json` containing
// {pid, port}. We watch that path for create/change, then connect to the
// reported port. On disconnect we wait for the discovery file to reappear and
// reconnect.

import * as fs from "fs";
import * as net from "net";
import * as path from "path";
import * as vscode from "vscode";
import {
  createMessageConnection,
  MessageConnection,
  SocketMessageReader,
  SocketMessageWriter,
} from "vscode-jsonrpc/node";

export interface DiscoveryFile {
  pid: number;
  port: number;
  version: number;
}

export type ConnectionState = "disconnected" | "connecting" | "connected";

export class RpcClient {
  private connection: MessageConnection | null = null;
  private socket: net.Socket | null = null;
  private watchers: fs.FSWatcher[] = [];
  private state: ConnectionState = "disconnected";
  private readonly _onState = new vscode.EventEmitter<ConnectionState>();
  readonly onState = this._onState.event;
  private readonly discoveryPaths: string[];

  constructor(projectRoot: string, hint?: string) {
    const candidates: string[] = [];
    for (const cfg of ["Debug", "Release"]) {
      candidates.push(path.join(projectRoot, "build", `project_${cfg}`, "tmp", "editor_rpc.json"));
    }
    if (hint && !candidates.includes(hint)) {
      candidates.unshift(hint);
    }
    this.discoveryPaths = candidates;
  }

  start(): void {
    this.tryConnectFromDiscovery();
    this.watchDiscovery();
  }

  dispose(): void {
    for (const w of this.watchers) w.close();
    this.watchers = [];
    this.disconnect();
    this._onState.dispose();
  }

  getState(): ConnectionState {
    return this.state;
  }

  // Send a request and wait for the response.
  request<T = unknown>(method: string, params?: unknown): Promise<T> {
    if (!this.connection) {
      return Promise.reject(new Error("RPC: not connected"));
    }
    return this.connection.sendRequest<T>(method, params);
  }

  // Subscribe to a notification from the engine. Returns a disposable.
  onNotification(method: string, handler: (params: any) => void): vscode.Disposable {
    if (!this.connection) {
      // Defer registration until connected. Re-register every time a new
      // connection is established.
      const pending = (conn: MessageConnection) =>
        conn.onNotification(method, handler);
      this.pendingNotificationHandlers.push({ method, handler, registrar: pending });
      return new vscode.Disposable(() => {
        // Best-effort removal — we don't track per-connection disposables.
      });
    }
    const sub = this.connection.onNotification(method, handler);
    return new vscode.Disposable(() => sub.dispose());
  }

  private pendingNotificationHandlers: Array<{
    method: string;
    handler: (params: any) => void;
    registrar: (conn: MessageConnection) => void;
  }> = [];

  private setState(s: ConnectionState): void {
    if (this.state !== s) {
      this.state = s;
      this._onState.fire(s);
    }
  }

  private watchDiscovery(): void {
    const dirs = new Set(this.discoveryPaths.map((p) => path.dirname(p)));
    for (const dir of dirs) {
      try {
        fs.mkdirSync(dir, { recursive: true });
      } catch {
        continue;
      }
      try {
        const w = fs.watch(dir, (event, filename) => {
          if (filename && filename === "editor_rpc.json") {
            if (this.state === "disconnected") {
              this.tryConnectFromDiscovery();
            }
          }
        });
        this.watchers.push(w);
      } catch (e) {
        console.error("kryga: fs.watch failed for", dir, e);
      }
    }
  }

  private tryConnectFromDiscovery(): void {
    if (this.state !== "disconnected") {
      return;
    }
    for (const dp of this.discoveryPaths) {
      try {
        const raw = fs.readFileSync(dp, "utf8");
        const info: DiscoveryFile = JSON.parse(raw);
        if (typeof info.port !== "number" || info.port <= 0) {
          continue;
        }
        this.connect(info.port);
        return;
      } catch {
        continue;
      }
    }
  }

  private connect(port: number): void {
    this.setState("connecting");
    const sock = net.createConnection({ host: "127.0.0.1", port }, () => {
      const reader = new SocketMessageReader(sock);
      const writer = new SocketMessageWriter(sock);
      const conn = createMessageConnection(reader, writer);
      this.connection = conn;
      this.socket = sock;

      for (const p of this.pendingNotificationHandlers) {
        p.registrar(conn);
      }

      conn.onClose(() => this.handleDisconnect());
      conn.onError(() => this.handleDisconnect());
      conn.listen();
      this.setState("connected");
    });
    sock.on("error", () => this.handleDisconnect());
  }

  private handleDisconnect(): void {
    if (this.connection) {
      try {
        this.connection.dispose();
      } catch {
        // ignore
      }
      this.connection = null;
    }
    if (this.socket) {
      try {
        this.socket.destroy();
      } catch {
        // ignore
      }
      this.socket = null;
    }
    this.setState("disconnected");
    // The watcher will trigger reconnect on the next discovery-file write.
    // If the engine is already up but the file already exists, we still want
    // to retry — schedule one-shot retry after a short delay.
    setTimeout(() => this.tryConnectFromDiscovery(), 500);
  }

  private disconnect(): void {
    this.handleDisconnect();
  }
}
