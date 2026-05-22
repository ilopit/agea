import * as path from "path";
import * as fs from "fs";
import * as vscode from "vscode";
import { RpcClient } from "./rpc";
import { SceneTreeProvider, SceneNode } from "./sceneTree";
import { InspectorProvider } from "./inspector";
import { ActionProgressProvider } from "./actionProgress";
import { openRenderConfig, refreshRenderConfig } from "./renderConfig";
import { openBakeEditor } from "./bakeEditor";
import { openConverter } from "./converter";
import { HotReloadWatcher } from "./hotReload";
import { DiagnosticsManager } from "./diagnostics";

interface LogParams {
  level: "trace" | "debug" | "info" | "warn" | "error" | "fatal";
  text: string;
}

interface SelectionChangedParams {
  id: string;
}

let client: RpcClient | undefined;
let output: vscode.OutputChannel | undefined;
let statusItem: vscode.StatusBarItem | undefined;
let modeItem: vscode.StatusBarItem | undefined;
let currentMode: "edit" | "play" = "edit";
let engineTerminal: vscode.Terminal | undefined;

function findProjectRoot(): string | undefined {
  for (const folder of vscode.workspace.workspaceFolders ?? []) {
    const anchor = path.join(folder.uri.fsPath, "kryga.project");
    if (fs.existsSync(anchor)) {
      return folder.uri.fsPath;
    }
  }
  return vscode.workspace.workspaceFolders?.[0]?.uri.fsPath;
}

function updateStatus(state: "disconnected" | "connecting" | "connected"): void {
  if (!statusItem) {
    return;
  }
  switch (state) {
    case "connected":
      statusItem.text = "$(zap) Kryga: connected";
      statusItem.tooltip = "Connected to running engine.";
      break;
    case "connecting":
      statusItem.text = "$(sync~spin) Kryga: connecting…";
      statusItem.tooltip = "Connecting to engine.";
      break;
    case "disconnected":
      statusItem.text = "$(circle-slash) Kryga: offline";
      statusItem.tooltip = "Engine not running. Launch kryga_editor to connect.";
      break;
  }
  statusItem.show();

  if (modeItem) {
    if (state === "connected") {
      modeItem.show();
    } else {
      modeItem.hide();
    }
  }
}

function updateModeItem(): void {
  if (!modeItem) return;
  if (currentMode === "play") {
    modeItem.text = "$(debug-start) play";
    modeItem.tooltip = "Engine in play mode — click to switch to edit.";
  } else {
    modeItem.text = "$(edit) edit";
    modeItem.tooltip = "Engine in edit mode — click to enter play.";
  }
}

function findDiscoveryFile(root: string): string | undefined {
  for (const cfg of ["Debug", "Release"]) {
    const p = path.join(root, "build", `project_${cfg}`, "tmp", "editor_rpc.json");
    if (fs.existsSync(p)) return p;
  }
  return undefined;
}

function detectRunningEngine(root: string): number | undefined {
  try {
    const dp = findDiscoveryFile(root);
    if (!dp) return undefined;
    const info = JSON.parse(fs.readFileSync(dp, "utf8")) as { pid: number };
    if (typeof info.pid !== "number") return undefined;
    process.kill(info.pid, 0);
    return info.pid;
  } catch {
    return undefined;
  }
}

function findEngineBinary(root: string): string | undefined {
  const exe = process.platform === "win32" ? "kryga_editor.exe" : "kryga_editor";
  for (const cfg of ["Debug", "Release"]) {
    const p = path.join(root, "build", `project_${cfg}`, "bin", exe);
    if (fs.existsSync(p)) return p;
  }
  return undefined;
}

export function activate(context: vscode.ExtensionContext): void {
  const root = findProjectRoot();
  if (!root) {
    vscode.window.showWarningMessage(
      "Kryga: no workspace folder open — extension idle.",
    );
    return;
  }

  output = vscode.window.createOutputChannel("Kryga Engine");
  context.subscriptions.push(output);

  statusItem = vscode.window.createStatusBarItem(
    vscode.StatusBarAlignment.Right,
    100,
  );
  statusItem.command = "kryga.showOutput";
  context.subscriptions.push(statusItem);

  modeItem = vscode.window.createStatusBarItem(
    vscode.StatusBarAlignment.Right,
    99,
  );
  modeItem.command = "kryga.engine.toggleMode";
  context.subscriptions.push(modeItem);
  updateModeItem();

  const discoveryPath = findDiscoveryFile(root)
    ?? path.join(root, "build", "project_Debug", "tmp", "editor_rpc.json");
  client = new RpcClient(root, discoveryPath);
  context.subscriptions.push({ dispose: () => client?.dispose() });

  client.onState((state) => updateStatus(state));
  updateStatus(client.getState());

  client.onNotification("log", (p: LogParams) => {
    const tag = p.level.toUpperCase();
    output?.appendLine(`[${tag}] ${p.text}`);
  });

  // --- Inspector ---
  const inspectorProvider = new InspectorProvider(client);
  context.subscriptions.push(
    vscode.window.registerWebviewViewProvider(
      "kryga.inspector",
      inspectorProvider,
    ),
  );

  // --- Action Progress ---
  const actionProgressProvider = new ActionProgressProvider(client);
  context.subscriptions.push(
    vscode.window.registerWebviewViewProvider(
      "kryga.actionProgress",
      actionProgressProvider,
    ),
  );

  client.onNotification("model.selection.changed", (p: SelectionChangedParams) => {
    output?.appendLine(`[selection] ${p.id || "(none)"}`);
    inspectorProvider.setSelection(p.id || undefined);
  });

  client.onNotification("model.object.property.changed", (p: any) => {
    inspectorProvider.reconcile(p);
  });

  client.onNotification("engine.mode.changed", (p: { mode: "edit" | "play" }) => {
    currentMode = p.mode;
    updateModeItem();
  });

  // --- Scene Tree ---
  const sceneProvider = new SceneTreeProvider(client);
  context.subscriptions.push(
    vscode.window.registerTreeDataProvider("kryga.scene", sceneProvider),
  );

  client.onNotification("model.scene.changed", () => sceneProvider.refresh());
  client.onState(async (state) => {
    sceneProvider.refresh();
    if (state !== "connected") {
      inspectorProvider.setSelection(undefined);
    } else {
      try {
        const r = await client?.request<{ mode: "edit" | "play" }>("engine.getMode");
        if (r) {
          currentMode = r.mode;
          updateModeItem();
        }
      } catch {
        // engine may not have getMode in older builds
      }
      refreshRenderConfig(client!);
    }
  });

  // --- Hot Reload ---
  const hotReload = new HotReloadWatcher(client, root);
  context.subscriptions.push({ dispose: () => hotReload.dispose() });

  // --- Diagnostics ---
  const diagnostics = new DiagnosticsManager(client, root);
  context.subscriptions.push({ dispose: () => diagnostics.dispose() });

  // --- Commands ---

  context.subscriptions.push(
    vscode.commands.registerCommand("kryga.scene.refresh", () =>
      sceneProvider.refresh(),
    ),
  );

  context.subscriptions.push(
    vscode.commands.registerCommand("kryga.scene.filter", async () => {
      const current = sceneProvider.getFilter();
      const value = await vscode.window.showInputBox({
        prompt: "Filter scene tree by name",
        value: current,
        placeHolder: "Type to filter…",
      });
      if (value !== undefined) {
        sceneProvider.setFilter(value);
      }
    }),
  );

  context.subscriptions.push(
    vscode.commands.registerCommand("kryga.scene.clearFilter", () => {
      sceneProvider.setFilter("");
    }),
  );

  context.subscriptions.push(
    vscode.commands.registerCommand("kryga.scene.select", async (id: string) => {
      try {
        await client?.request("model.selection.set", { id });
      } catch (e) {
        vscode.window.showErrorMessage(`Kryga: model.selection.set failed: ${e}`);
      }
    }),
  );

  context.subscriptions.push(
    vscode.commands.registerCommand("kryga.scene.create", async () => {
      const name = await vscode.window.showInputBox({
        prompt: "New game object name",
        placeHolder: "my_object",
      });
      if (!name) return;
      try {
        await client?.request("model.scene.create", { name });
        sceneProvider.refresh();
      } catch (e) {
        vscode.window.showErrorMessage(`Kryga: create failed — ${e}`);
      }
    }),
  );

  context.subscriptions.push(
    vscode.commands.registerCommand("kryga.scene.duplicate", async (node: SceneNode) => {
      const id = node?.id;
      if (!id) return;
      try {
        await client?.request("model.scene.duplicate", { id });
        sceneProvider.refresh();
      } catch (e) {
        vscode.window.showErrorMessage(`Kryga: duplicate failed — ${e}`);
      }
    }),
  );

  context.subscriptions.push(
    vscode.commands.registerCommand("kryga.scene.rename", async (node: SceneNode) => {
      const id = node?.id;
      if (!id) return;
      const newName = await vscode.window.showInputBox({
        prompt: `Rename "${node.label}"`,
        value: node.label,
      });
      if (!newName || newName === node.label) return;
      try {
        await client?.request("model.scene.rename", { id, name: newName });
        sceneProvider.refresh();
      } catch (e) {
        vscode.window.showErrorMessage(`Kryga: rename failed — ${e}`);
      }
    }),
  );

  context.subscriptions.push(
    vscode.commands.registerCommand("kryga.scene.delete", async (node: SceneNode) => {
      const id = node?.id;
      if (!id) return;
      const confirm = await vscode.window.showWarningMessage(
        `Delete "${node.label}"?`,
        { modal: true },
        "Delete",
      );
      if (confirm !== "Delete") return;
      try {
        await client?.request("model.scene.delete", { id });
        sceneProvider.refresh();
      } catch (e) {
        vscode.window.showErrorMessage(`Kryga: delete failed — ${e}`);
      }
    }),
  );

  context.subscriptions.push(
    vscode.commands.registerCommand("kryga.renderConfig", () => {
      openRenderConfig(client!);
    }),
  );

  context.subscriptions.push(
    vscode.commands.registerCommand("kryga.bakeEditor", () => {
      openBakeEditor(client!);
    }),
  );

  context.subscriptions.push(
    vscode.commands.registerCommand("kryga.converter", () => {
      openConverter(client!);
    }),
  );

  context.subscriptions.push(
    vscode.commands.registerCommand("kryga.connect", () => {
      client?.start();
    }),
  );

  context.subscriptions.push(
    vscode.commands.registerCommand("kryga.disconnect", () => {
      client?.dispose();
      client = new RpcClient(root, discoveryPath);
      client.onState((state) => updateStatus(state));
      updateStatus(client.getState());
    }),
  );

  context.subscriptions.push(
    vscode.commands.registerCommand("kryga.ping", async () => {
      try {
        const result = await client?.request("ping", { hello: "vscode" });
        vscode.window.showInformationMessage(
          `Kryga ping: ${JSON.stringify(result)}`,
        );
      } catch (e) {
        vscode.window.showErrorMessage(`Kryga ping failed: ${e}`);
      }
    }),
  );

  context.subscriptions.push(
    vscode.commands.registerCommand("kryga.showOutput", () => output?.show()),
  );

  context.subscriptions.push(
    vscode.commands.registerCommand("kryga.level.load", async () => {
      try {
        const list = await client?.request<{ levels: string[]; current: string }>(
          "model.level.list",
        );
        if (!list || list.levels.length === 0) {
          vscode.window.showInformationMessage("Kryga: no levels found.");
          return;
        }
        const items = list.levels.map((id) => ({
          label: id,
          description: id === list.current ? "(current)" : "",
        }));
        const pick = await vscode.window.showQuickPick(items, {
          placeHolder: "Select a level to load",
        });
        if (!pick) return;
        await client?.request("model.level.load", { id: pick.label });
      } catch (e) {
        vscode.window.showErrorMessage(`Kryga: model.level.load failed — ${e}`);
      }
    }),
  );

  context.subscriptions.push(
    vscode.commands.registerCommand("kryga.engine.toggleMode", async () => {
      const target = currentMode === "play" ? "edit" : "play";
      try {
        await client?.request("engine.setMode", { mode: target });
      } catch (e) {
        vscode.window.showErrorMessage(`Kryga: setMode failed — ${e}`);
      }
    }),
  );

  context.subscriptions.push(
    vscode.commands.registerCommand("kryga.engine.editMode", async () => {
      try {
        await client?.request("engine.setMode", { mode: "edit" });
      } catch (e) {
        vscode.window.showErrorMessage(`Kryga: setMode(edit) failed — ${e}`);
      }
    }),
  );

  context.subscriptions.push(
    vscode.commands.registerCommand("kryga.engine.playMode", async () => {
      try {
        await client?.request("engine.setMode", { mode: "play" });
      } catch (e) {
        vscode.window.showErrorMessage(`Kryga: setMode(play) failed — ${e}`);
      }
    }),
  );

  context.subscriptions.push(
    vscode.commands.registerCommand("kryga.engine.start", async () => {
      const existing = detectRunningEngine(root);
      if (existing !== undefined) {
        vscode.window.showInformationMessage(
          `Kryga: engine already running (PID ${existing}). Use "Kryga: Stop engine" first.`,
        );
        return;
      }
      const exe = findEngineBinary(root);
      if (!exe) {
        vscode.window.showErrorMessage(
          "Kryga: kryga_editor not found under build/project_Debug or build/project_Release. Build first.",
        );
        return;
      }
      const cwd = path.dirname(exe);
      if (engineTerminal && engineTerminal.exitStatus !== undefined) {
        engineTerminal = undefined;
      }
      engineTerminal = vscode.window.createTerminal({
        name: "Kryga Engine",
        cwd,
      });
      const cmd =
        process.platform === "win32"
          ? `& "${exe}"`
          : `"${exe}"`;
      engineTerminal.sendText(cmd);
      engineTerminal.show();
    }),
  );

  context.subscriptions.push(
    vscode.commands.registerCommand("kryga.engine.stop", async () => {
      const pid = detectRunningEngine(root);
      if (pid === undefined) {
        vscode.window.showInformationMessage("Kryga: engine not running.");
        return;
      }
      if (client?.getState() === "connected") {
        try {
          await client.request("engine.shutdown");
          return;
        } catch {
          // RPC failed — fall through to hard kill
        }
      }
      try {
        process.kill(pid, "SIGTERM");
      } catch (e) {
        vscode.window.showErrorMessage(`Kryga: kill PID ${pid} failed — ${e}`);
      }
    }),
  );

  client.start();
}

export function deactivate(): void {
  client?.dispose();
  client = undefined;
}
