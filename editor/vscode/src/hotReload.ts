import * as path from "path";
import * as vscode from "vscode";
import { RpcClient } from "./rpc";

export class HotReloadWatcher {
  private watcher: vscode.FileSystemWatcher | undefined;
  private disposables: vscode.Disposable[] = [];

  constructor(
    private readonly client: RpcClient,
    private readonly root: string,
  ) {
    this.setup();
    vscode.workspace.onDidChangeConfiguration((e) => {
      if (e.affectsConfiguration("kryga.hotReload")) {
        this.dispose();
        this.setup();
      }
    }, undefined, this.disposables);
  }

  private setup(): void {
    const cfg = vscode.workspace.getConfiguration("kryga.hotReload");
    if (!cfg.get<boolean>("enabled", true)) return;

    const exts = cfg.get<string[]>("extensions", [".vert", ".frag", ".comp", ".lua", ".glsl"]);
    const pattern = `**/*{${exts.join(",")}}`;

    this.watcher = vscode.workspace.createFileSystemWatcher(pattern);
    this.watcher.onDidChange((uri) => this.reload(uri));
    this.watcher.onDidCreate((uri) => this.reload(uri));
    this.disposables.push(this.watcher);
  }

  private reload(uri: vscode.Uri): void {
    if (this.client.getState() !== "connected") return;

    const rel = path.relative(this.root, uri.fsPath).replace(/\\/g, "/");
    this.client.request("sync.reload", { path: uri.fsPath }).catch(() => {
      // Engine may not be ready — silently ignore.
    });
  }

  dispose(): void {
    for (const d of this.disposables) d.dispose();
    this.disposables = [];
    this.watcher = undefined;
  }
}
