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
  private nodesById = new Map<string, SceneNode>();
  private parentOf = new Map<string, string>();

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

  getNode(id: string): SceneNode | undefined {
    return this.nodesById.get(id);
  }

  getGameObjectId(id: string): string {
    return this.parentOf.get(id) ?? id;
  }

  getParent(node: SceneNode): SceneNode | undefined {
    const parentId = this.parentOf.get(node.id);
    return parentId ? this.nodesById.get(parentId) : undefined;
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
      arguments: [node.id, node.kind],
    };
    return item;
  }

  async getChildren(parent?: SceneNode): Promise<SceneNode[]> {
    if (this.client.getState() !== "connected") {
      return [];
    }
    try {
      if (!parent) {
        this.nodesById.clear();
        this.parentOf.clear();
        const root = await this.client.request<GetRootResult>("model.scene.getRoot");
        let children = root.children ?? [];
        if (this.filter) {
          children = children.filter(
            (n) => n.label.toLowerCase().includes(this.filter) ||
                   n.id.toLowerCase().includes(this.filter),
          );
        }
        for (const c of children) this.nodesById.set(c.id, c);
        return children;
      }
      const sub = await this.client.request<GetChildrenResult>("model.scene.getChildren", {
        id: parent.id,
      });
      const children = sub.children ?? [];
      const goId = parent.kind === "game_object" ? parent.id : (this.parentOf.get(parent.id) ?? parent.id);
      for (const c of children) {
        this.nodesById.set(c.id, c);
        this.parentOf.set(c.id, goId);
      }
      return children;
    } catch (e) {
      console.error("kryga: scene RPC failed", e);
      return [];
    }
  }
}
