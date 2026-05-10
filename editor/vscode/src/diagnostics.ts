import * as path from "path";
import * as vscode from "vscode";
import { RpcClient } from "./rpc";

interface ShaderDiagnostic {
  file: string;
  line: number;
  column: number;
  severity: "error" | "warning";
  message: string;
}

interface DiagnosticsParams {
  diagnostics: ShaderDiagnostic[];
}

export class DiagnosticsManager {
  private readonly collection: vscode.DiagnosticCollection;

  constructor(client: RpcClient, private readonly root: string) {
    this.collection = vscode.languages.createDiagnosticCollection("kryga");

    client.onNotification("diagnostics.shader", (p: DiagnosticsParams) => {
      this.update(p.diagnostics);
    });

    client.onNotification("diagnostics.clear", () => {
      this.collection.clear();
    });
  }

  dispose(): void {
    this.collection.dispose();
  }

  update(items: ShaderDiagnostic[]): void {
    const byFile = new Map<string, vscode.Diagnostic[]>();

    for (const d of items) {
      const absPath = path.isAbsolute(d.file)
        ? d.file
        : path.join(this.root, d.file);
      const uri = vscode.Uri.file(absPath);
      const key = uri.toString();

      const line = Math.max(0, d.line - 1);
      const col = Math.max(0, d.column - 1);
      const range = new vscode.Range(line, col, line, 200);
      const severity = d.severity === "error"
        ? vscode.DiagnosticSeverity.Error
        : vscode.DiagnosticSeverity.Warning;

      const diag = new vscode.Diagnostic(range, d.message, severity);
      diag.source = "kryga-glslc";

      if (!byFile.has(key)) {
        byFile.set(key, []);
      }
      byFile.get(key)!.push(diag);
    }

    this.collection.clear();
    for (const [key, diags] of byFile) {
      this.collection.set(vscode.Uri.parse(key), diags);
    }
  }
}
