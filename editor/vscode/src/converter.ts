import * as vscode from "vscode";
import { RpcClient } from "./rpc";

let currentPanel: vscode.WebviewPanel | undefined;

export function openConverter(client: RpcClient): void {
  if (currentPanel) {
    currentPanel.reveal();
    return;
  }

  currentPanel = vscode.window.createWebviewPanel(
    "kryga.converter",
    "Asset Converter",
    vscode.ViewColumn.One,
    { enableScripts: true, retainContextWhenHidden: true },
  );

  currentPanel.webview.html = html();

  currentPanel.webview.onDidReceiveMessage(async (msg) => {
    switch (msg?.type) {
      case "ready":
        break;
      case "browseInput": {
        const uris = await vscode.window.showOpenDialog({
          canSelectFiles: true,
          canSelectFolders: false,
          filters: { "3D Assets": ["glb", "gltf", "obj"], "All files": ["*"] },
        });
        if (uris && uris.length > 0) {
          currentPanel?.webview.postMessage({ type: "setInput", path: uris[0].fsPath });
        }
        break;
      }
      case "browseOutput": {
        const uris = await vscode.window.showOpenDialog({
          canSelectFiles: false,
          canSelectFolders: true,
        });
        if (uris && uris.length > 0) {
          currentPanel?.webview.postMessage({ type: "setOutput", path: uris[0].fsPath });
        }
        break;
      }
      case "refreshDeps": {
        try {
          const result = await client.request<{ deps: Array<{ id: string; checked: boolean }> }>(
            "converter.listDeps",
            { output_root: msg.output_root },
          );
          currentPanel?.webview.postMessage({ type: "loadDeps", deps: result.deps });
        } catch (e) {
          vscode.window.showErrorMessage(`Kryga: listDeps failed — ${e}`);
        }
        break;
      }
      case "convert": {
        try {
          await client.request("converter.start", msg.params);
          currentPanel?.webview.postMessage({ type: "running" });
        } catch (e) {
          currentPanel?.webview.postMessage({ type: "convError", message: String(e) });
        }
        break;
      }
    }
  });

  client.onNotification("converter.completed", (p: any) => {
    currentPanel?.webview.postMessage({
      type: "completed",
      success: p.success,
      exit_code: p.exit_code,
      log_tail: p.log_tail,
    });
  });

  currentPanel.onDidDispose(() => {
    currentPanel = undefined;
  });
}

function html(): string {
  return /* html */ `
<!DOCTYPE html>
<html>
<head>
  <meta charset="UTF-8" />
  <meta http-equiv="Content-Security-Policy"
        content="default-src 'none'; style-src 'unsafe-inline'; script-src 'unsafe-inline';">
  <style>
    :root { color-scheme: var(--vscode-color-scheme); }
    body {
      margin: 0; padding: 12px 20px;
      font-family: var(--vscode-font-family);
      font-size: var(--vscode-font-size);
      color: var(--vscode-foreground);
      background: var(--vscode-editor-background);
      max-width: 700px;
    }
    h2 { margin: 0 0 8px 0; font-size: 1.15em; }

    .field {
      display: grid;
      grid-template-columns: 140px 1fr auto;
      align-items: center; gap: 8px;
      padding: 4px 0; min-height: 28px;
    }
    .field-label {
      overflow: hidden; text-overflow: ellipsis; white-space: nowrap;
    }
    .field input[type="text"], .field select {
      flex: 1; width: 100%;
      padding: 3px 6px;
      color: var(--vscode-input-foreground);
      background: var(--vscode-input-background);
      border: 1px solid var(--vscode-input-border, transparent);
      border-radius: 2px; font-family: inherit; font-size: inherit;
      box-sizing: border-box;
    }

    .browse-btn, .convert-btn, .refresh-btn {
      padding: 3px 10px;
      color: var(--vscode-button-foreground);
      background: var(--vscode-button-background);
      border: none; border-radius: 3px;
      cursor: pointer; font-size: 0.9em;
      white-space: nowrap;
    }
    .browse-btn:hover, .convert-btn:hover, .refresh-btn:hover {
      background: var(--vscode-button-hoverBackground);
    }
    .convert-btn { margin-top: 10px; padding: 6px 24px; font-weight: 600; }
    .convert-btn:disabled { opacity: 0.5; cursor: not-allowed; }

    .deps-section { margin-top: 8px; }
    .deps-header { display: flex; align-items: center; gap: 8px; font-weight: 600; }
    .deps-list {
      max-height: 150px; overflow-y: auto;
      border: 1px solid var(--vscode-input-border, transparent);
      border-radius: 3px; padding: 4px 8px; margin-top: 4px;
      background: var(--vscode-input-background);
    }
    .dep-item { padding: 2px 0; display: flex; align-items: center; gap: 6px; }
    .dep-item.locked { opacity: 0.6; }

    .status-area {
      margin-top: 10px; padding: 8px;
      border-radius: 3px; font-size: 0.9em;
      white-space: pre-wrap; word-break: break-all;
      max-height: 200px; overflow-y: auto;
    }
    .status-ok {
      background: var(--vscode-diffEditor-insertedTextBackground, rgba(0,200,0,0.1));
      color: var(--vscode-foreground);
    }
    .status-err {
      background: var(--vscode-diffEditor-removedTextBackground, rgba(200,0,0,0.1));
      color: var(--vscode-foreground);
    }
    .status-running {
      background: var(--vscode-editor-inactiveSelectionBackground);
      color: var(--vscode-foreground);
    }
  </style>
</head>
<body>
  <h2>Asset Converter</h2>
  <div id="root"></div>
  <script>
    const vscode = acquireVsCodeApi();
    const root = document.getElementById('root');

    const MODES = ['package', 'extend', 'level', 'level+package'];
    const ALWAYS_ON = new Set(['root', 'base']);

    let form = { input: '', output: '', name: '', mode: 'package', existing: '' };
    let deps = [];
    let running = false;
    let statusText = '';
    let statusType = '';

    vscode.postMessage({ type: 'ready' });

    window.addEventListener('message', (ev) => {
      const m = ev.data;
      switch (m.type) {
        case 'setInput':
          form.input = m.path;
          render();
          break;
        case 'setOutput':
          form.output = m.path;
          vscode.postMessage({ type: 'refreshDeps', output_root: m.path });
          render();
          break;
        case 'loadDeps':
          deps = m.deps;
          render();
          break;
        case 'running':
          running = true;
          statusText = 'Running...';
          statusType = 'running';
          render();
          break;
        case 'completed':
          running = false;
          statusText = (m.success ? 'OK' : 'ERROR (exit ' + m.exit_code + ')') +
            (m.log_tail ? '\\n' + m.log_tail : '');
          statusType = m.success ? 'ok' : 'err';
          render();
          break;
        case 'convError':
          running = false;
          statusText = m.message;
          statusType = 'err';
          render();
          break;
      }
    });

    render();

    function render() {
      root.innerHTML = '';

      // Input
      addPathField('Input file', form.input, 'browseInput', (v) => form.input = v);
      addPathField('Output root', form.output, 'browseOutput', (v) => {
        form.output = v;
        vscode.postMessage({ type: 'refreshDeps', output_root: v });
      });

      // Name
      const nameField = document.createElement('div');
      nameField.className = 'field';
      nameField.innerHTML = '<div class="field-label">Name</div>';
      const nameInput = document.createElement('input');
      nameInput.type = 'text';
      nameInput.value = form.name;
      nameInput.placeholder = 'asset_id';
      nameInput.addEventListener('change', () => form.name = nameInput.value);
      nameField.appendChild(nameInput);
      nameField.appendChild(document.createElement('div'));
      root.appendChild(nameField);

      // Mode
      const modeField = document.createElement('div');
      modeField.className = 'field';
      modeField.innerHTML = '<div class="field-label">Mode</div>';
      const sel = document.createElement('select');
      for (const m of MODES) {
        const o = document.createElement('option');
        o.value = m; o.textContent = m;
        if (m === form.mode) o.selected = true;
        sel.appendChild(o);
      }
      sel.addEventListener('change', () => { form.mode = sel.value; render(); });
      modeField.appendChild(sel);
      modeField.appendChild(document.createElement('div'));
      root.appendChild(modeField);

      // Existing package (for extend/level)
      if (form.mode === 'extend' || form.mode === 'level') {
        const exField = document.createElement('div');
        exField.className = 'field';
        exField.innerHTML = '<div class="field-label">Existing package</div>';
        const exInput = document.createElement('input');
        exInput.type = 'text';
        exInput.value = form.existing;
        exInput.addEventListener('change', () => form.existing = exInput.value);
        exField.appendChild(exInput);
        exField.appendChild(document.createElement('div'));
        root.appendChild(exField);
      }

      // Dependencies
      const depSec = document.createElement('div');
      depSec.className = 'deps-section';
      const depHdr = document.createElement('div');
      depHdr.className = 'deps-header';
      depHdr.textContent = 'Dependencies ';
      const refBtn = document.createElement('button');
      refBtn.className = 'refresh-btn';
      refBtn.textContent = 'Refresh';
      refBtn.addEventListener('click', () => {
        vscode.postMessage({ type: 'refreshDeps', output_root: form.output });
      });
      depHdr.appendChild(refBtn);
      depSec.appendChild(depHdr);

      if (deps.length > 0) {
        const list = document.createElement('div');
        list.className = 'deps-list';
        for (const d of deps) {
          const item = document.createElement('div');
          item.className = 'dep-item';
          const locked = ALWAYS_ON.has(d.id);
          if (locked) item.classList.add('locked');
          const cb = document.createElement('input');
          cb.type = 'checkbox';
          cb.checked = d.checked;
          cb.disabled = running || locked;
          cb.addEventListener('change', () => { d.checked = cb.checked; });
          item.appendChild(cb);
          const label = document.createElement('span');
          label.textContent = d.id;
          item.appendChild(label);
          list.appendChild(item);
        }
        depSec.appendChild(list);
      }
      root.appendChild(depSec);

      // Convert button
      const btn = document.createElement('button');
      btn.className = 'convert-btn';
      btn.textContent = running ? 'Converting...' : 'Convert';
      btn.disabled = running;
      btn.addEventListener('click', () => {
        const checkedDeps = deps.filter(d => d.checked).map(d => d.id);
        vscode.postMessage({
          type: 'convert',
          params: {
            input: form.input,
            output_root: form.output,
            name: form.name,
            mode: form.mode,
            existing_package: form.existing,
            deps: checkedDeps
          }
        });
      });
      root.appendChild(btn);

      // Status
      if (statusText) {
        const area = document.createElement('div');
        area.className = 'status-area status-' + statusType;
        area.textContent = statusText;
        root.appendChild(area);
      }
    }

    function addPathField(label, value, browseType, onChange) {
      const field = document.createElement('div');
      field.className = 'field';
      field.innerHTML = '<div class="field-label">' + label + '</div>';
      const inp = document.createElement('input');
      inp.type = 'text';
      inp.value = value;
      inp.disabled = running;
      inp.addEventListener('change', () => onChange(inp.value));
      field.appendChild(inp);
      const btn = document.createElement('button');
      btn.className = 'browse-btn';
      btn.textContent = 'Browse';
      btn.disabled = running;
      btn.addEventListener('click', () => vscode.postMessage({ type: browseType }));
      field.appendChild(btn);
      root.appendChild(field);
    }
  </script>
</body>
</html>`;
}
