import * as vscode from "vscode";
import { RpcClient } from "./rpc";

export interface SceneNode {
  id: string;
  label: string;
  has_children: boolean;
  kind: "game_object" | "component";
  type_name?: string;
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
  private filter = "";

  constructor(private readonly client: RpcClient) {}

  refresh(): void {
    this._onDidChange.fire(undefined);
  }

  setFilter(text: string): void {
    this.filter = text.toLowerCase();
    this._onDidChange.fire(undefined);
  }

  getFilter(): string {
    return this.filter;
  }

  getTreeItem(node: SceneNode): vscode.TreeItem {
    const item = new vscode.TreeItem(
      node.label || "(unnamed)",
      node.has_children
        ? vscode.TreeItemCollapsibleState.Collapsed
        : vscode.TreeItemCollapsibleState.None,
    );
    item.id = node.id;
    item.tooltip = node.type_name
      ? `${node.id}\n${node.type_name}`
      : node.id;
    item.contextValue = node.kind === "game_object" ? "krygaObject" : "krygaComponent";
    if (node.kind === "game_object") {
      item.iconPath = new vscode.ThemeIcon("symbol-object");
    } else {
      item.iconPath = new vscode.ThemeIcon("symbol-property");
    }
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
        let children = root.children ?? [];
        if (this.filter) {
          children = children.filter(
            (n) => n.label.toLowerCase().includes(this.filter) ||
                   n.id.toLowerCase().includes(this.filter),
          );
        }
        return children;
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
