import * as vscode from "vscode";
import { RpcClient } from "./rpc";

let panel: vscode.WebviewPanel | undefined;

export function openMaterialBrowser(
  client: RpcClient,
  ownerId: string,
  currentMaterial: string,
): void {
  if (panel) {
    panel.reveal();
    panel.webview.postMessage({
      type: "init",
      owner_id: ownerId,
      current_material: currentMaterial,
    });
    return;
  }

  panel = vscode.window.createWebviewPanel(
    "kryga.materialBrowser",
    "Kryga: Materials",
    vscode.ViewColumn.Beside,
    { enableScripts: true },
  );

  panel.onDidDispose(() => {
    panel = undefined;
  });

  panel.webview.html = html();

  panel.webview.onDidReceiveMessage(async (msg) => {
    if (msg.type === "ready" || msg.type === "refresh") {
      try {
        const res = await client.request<{
          materials: { id: string; type: string }[];
        }>("material.list", {});
        panel?.webview.postMessage({
          type: "materials",
          materials: res.materials,
          owner_id: msg.owner_id,
          current_material: msg.current_material,
        });
      } catch (e) {
        panel?.webview.postMessage({
          type: "error",
          message: String(e),
        });
      }
    } else if (msg.type === "requestPreview") {
      try {
        const res = await client.request<{ image: string }>(
          "material.preview",
          { id: msg.material_id, size: 128 },
        );
        panel?.webview.postMessage({
          type: "previewResult",
          material_id: msg.material_id,
          image: res.image,
        });
      } catch {
        // preview unavailable
      }
    } else if (msg.type === "assign") {
      try {
        await client.request("material.assign", {
          owner_id: msg.owner_id,
          material_id: msg.material_id,
        });
        panel?.webview.postMessage({
          type: "assigned",
          material_id: msg.material_id,
        });
      } catch (e) {
        vscode.window.showErrorMessage(`Kryga: assign failed — ${e}`);
      }
    }
  });

  panel.webview.postMessage({
    type: "init",
    owner_id: ownerId,
    current_material: currentMaterial,
  });
}

function html(): string {
  return /* html */ `
<!DOCTYPE html>
<html>
<head>
  <meta charset="UTF-8" />
  <meta http-equiv="Content-Security-Policy"
        content="default-src 'none'; style-src 'unsafe-inline'; script-src 'unsafe-inline'; img-src data:;">
  <style>
    :root { color-scheme: var(--vscode-color-scheme); }
    body {
      margin: 0; padding: 12px;
      font-family: var(--vscode-font-family);
      font-size: var(--vscode-font-size);
      color: var(--vscode-foreground);
      background: var(--vscode-editor-background);
    }
    .header {
      display: flex;
      align-items: center;
      justify-content: space-between;
      margin-bottom: 12px;
      padding-bottom: 8px;
      border-bottom: 1px solid var(--vscode-sideBar-border);
    }
    .header h2 { margin: 0; font-size: 1.1em; }
    .current-label {
      font-size: 0.85em;
      color: var(--vscode-descriptionForeground);
    }
    .grid {
      display: grid;
      grid-template-columns: repeat(auto-fill, minmax(120px, 1fr));
      gap: 8px;
    }
    .mat-card {
      display: flex;
      flex-direction: column;
      align-items: center;
      padding: 8px;
      border: 2px solid transparent;
      border-radius: 6px;
      cursor: pointer;
      transition: background 0.1s;
    }
    .mat-card:hover {
      background: var(--vscode-list-hoverBackground);
    }
    .mat-card.active {
      border-color: var(--vscode-focusBorder);
      background: var(--vscode-list-activeSelectionBackground);
    }
    .mat-card .preview {
      width: 80px;
      height: 80px;
      border-radius: 50%;
      background: var(--vscode-editor-inactiveSelectionBackground);
      margin-bottom: 6px;
      display: flex;
      align-items: center;
      justify-content: center;
      overflow: hidden;
    }
    .mat-card .preview img {
      width: 80px;
      height: 80px;
      image-rendering: auto;
    }
    .mat-card .preview .placeholder {
      font-size: 0.75em;
      color: var(--vscode-descriptionForeground);
    }
    .mat-card .label {
      font-size: 0.8em;
      text-align: center;
      overflow: hidden;
      text-overflow: ellipsis;
      white-space: nowrap;
      max-width: 100%;
    }
    .err { color: var(--vscode-errorForeground); padding: 12px; }
    .loading { color: var(--vscode-descriptionForeground); padding: 12px; }
  </style>
</head>
<body>
  <div class="header">
    <h2>Materials</h2>
    <span class="current-label" id="current-label"></span>
  </div>
  <div id="root" class="loading">Loading materials...</div>
  <script>
    const vscode = acquireVsCodeApi();
    const root = document.getElementById('root');
    const currentLabel = document.getElementById('current-label');
    let ownerId = '';
    let currentMaterial = '';

    window.addEventListener('message', (ev) => {
      const m = ev.data;
      if (m.type === 'init') {
        ownerId = m.owner_id;
        currentMaterial = m.current_material;
        currentLabel.textContent = 'Current: ' + currentMaterial;
        vscode.postMessage({ type: 'refresh', owner_id: ownerId, current_material: currentMaterial });
      } else if (m.type === 'materials') {
        renderGrid(m.materials, m.current_material);
        for (const mat of m.materials) {
          vscode.postMessage({ type: 'requestPreview', material_id: mat.id });
        }
      } else if (m.type === 'previewResult') {
        const img = document.getElementById('preview-' + m.material_id);
        if (img) {
          img.src = m.image;
          img.style.display = '';
          const ph = img.parentElement.querySelector('.placeholder');
          if (ph) ph.style.display = 'none';
        }
      } else if (m.type === 'assigned') {
        currentMaterial = m.material_id;
        currentLabel.textContent = 'Current: ' + currentMaterial;
        document.querySelectorAll('.mat-card').forEach(c => c.classList.remove('active'));
        const card = document.getElementById('card-' + m.material_id);
        if (card) card.classList.add('active');
      } else if (m.type === 'error') {
        root.className = 'err';
        root.textContent = 'Error: ' + m.message;
      }
    });

    function renderGrid(materials, current) {
      root.className = 'grid';
      root.innerHTML = '';
      for (const mat of materials) {
        const card = document.createElement('div');
        card.className = 'mat-card';
        card.id = 'card-' + mat.id;
        if (mat.id === current) card.classList.add('active');

        const preview = document.createElement('div');
        preview.className = 'preview';
        const img = document.createElement('img');
        img.id = 'preview-' + mat.id;
        img.style.display = 'none';
        preview.appendChild(img);
        const ph = document.createElement('span');
        ph.className = 'placeholder';
        ph.textContent = '...';
        preview.appendChild(ph);
        card.appendChild(preview);

        const label = document.createElement('div');
        label.className = 'label';
        label.textContent = mat.id;
        label.title = mat.id + ' (' + mat.type + ')';
        card.appendChild(label);

        card.addEventListener('click', () => {
          vscode.postMessage({ type: 'assign', owner_id: ownerId, material_id: mat.id });
        });

        root.appendChild(card);
      }
    }

    vscode.postMessage({ type: 'ready' });
  </script>
</body>
</html>`;
}
