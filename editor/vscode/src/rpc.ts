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
  private watcher: fs.FSWatcher | null = null;
  private state: ConnectionState = "disconnected";
  private readonly _onState = new vscode.EventEmitter<ConnectionState>();
  readonly onState = this._onState.event;

  constructor(private readonly discoveryPath: string) {}

  start(): void {
    this.tryConnectFromDiscovery();
    this.watchDiscovery();
  }

  dispose(): void {
    this.watcher?.close();
    this.watcher = null;
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
    const dir = path.dirname(this.discoveryPath);
    try {
      fs.mkdirSync(dir, { recursive: true });
    } catch {
      // ignore — fs.watch will surface the real error
    }
    try {
      this.watcher = fs.watch(dir, (event, filename) => {
        if (filename && path.resolve(dir, filename) === this.discoveryPath) {
          if (this.state === "disconnected") {
            this.tryConnectFromDiscovery();
          }
        }
      });
    } catch (e) {
      console.error("kryga: fs.watch failed", e);
    }
  }

  private tryConnectFromDiscovery(): void {
    if (this.state !== "disconnected") {
      return;
    }
    let info: DiscoveryFile;
    try {
      const raw = fs.readFileSync(this.discoveryPath, "utf8");
      info = JSON.parse(raw);
    } catch {
      // No discovery file yet — wait.
      return;
    }
    if (typeof info.port !== "number" || info.port <= 0) {
      return;
    }
    this.connect(info.port);
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
