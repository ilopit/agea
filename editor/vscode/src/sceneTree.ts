// Scene tree fed by the engine over RPC.
//
// Phase B: flat list of game_objects under a single level node. The engine's
// `scene.getRoot` returns the level id and a children array of {id, label,
// has_children}; `scene.getChildren` is currently always empty (component
// nesting comes later). Refresh is triggered by the `scene.changed`
// notification from the engine, which fires when the object count changes.

import * as vscode from "vscode";
import { RpcClient } from "./rpc";

interface SceneNode {
  id: string;
  label: string;
  has_children: boolean;
}

interface GetRootResult {
  level: string;
  children: SceneNode[];
}

interface GetChildrenResult {
  children: SceneNode[];
}

export class SceneTreeProvider implements vscode.TreeDataProvider<SceneNode> {
  private readonly _onDidChange = new vscode.EventEmitter<SceneNode | undefined>();
  readonly onDidChangeTreeData = this._onDidChange.event;

  constructor(private readonly client: RpcClient) {}

  refresh(): void {
    this._onDidChange.fire(undefined);
  }

  getTreeItem(node: SceneNode): vscode.TreeItem {
    const item = new vscode.TreeItem(
      node.label || "(unnamed)",
      node.has_children
        ? vscode.TreeItemCollapsibleState.Collapsed
        : vscode.TreeItemCollapsibleState.None,
    );
    item.id = node.id;
    item.tooltip = node.id;
    item.contextValue = "krygaObject";
    item.command = {
      command: "kryga.scene.select",
      title: "Select",
      arguments: [node.id],
    };
    return item;
  }

  async getChildren(parent?: SceneNode): Promise<SceneNode[]> {
    if (this.client.getState() !== "connected") {
      return [];
    }
    try {
      if (!parent) {
        const root = await this.client.request<GetRootResult>("scene.getRoot");
        return root.children ?? [];
      }
      const sub = await this.client.request<GetChildrenResult>("scene.getChildren", {
        id: parent.id,
      });
      return sub.children ?? [];
    } catch (e) {
      console.error("kryga: scene RPC failed", e);
      return [];
    }
  }
}
