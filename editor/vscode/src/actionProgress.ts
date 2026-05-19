import * as vscode from "vscode";
import { RpcClient } from "./rpc";

interface ActionResult {
  name: string;
  success: boolean;
  error?: string;
  duration_ms: number;
}

interface ActionStatus {
  busy: boolean;
  current_name: string;
  progress: number;
  status: string;
  queued_count: number;
  finished: ActionResult[];
}

export class ActionProgressProvider implements vscode.WebviewViewProvider {
  private view?: vscode.WebviewView;

  constructor(private readonly client: RpcClient) {
    client.onNotification("tools.action.started", (p: any) => {
      this.post({ type: "started", name: p.name });
    });
    client.onNotification("tools.action.progress", (p: any) => {
      this.post({ type: "progress", name: p.name, progress: p.progress, status: p.status });
    });
    client.onNotification("tools.action.completed", (p: any) => {
      this.post({ type: "completed", name: p.name, success: p.success, error: p.error, duration_ms: p.duration_ms });
    });
  }

  resolveWebviewView(view: vscode.WebviewView): void {
    this.view = view;
    view.webview.options = { enableScripts: true };
    view.webview.html = html();

    view.webview.onDidReceiveMessage(async (msg) => {
      if (msg?.type === "ready") {
        this.fetchStatus();
      } else if (msg?.type === "clearFinished") {
        try {
          await this.client.request("tools.actions.clearFinished");
          this.fetchStatus();
        } catch {
          // ignore
        }
      }
    });
  }

  private async fetchStatus(): Promise<void> {
    try {
      const status = await this.client.request<ActionStatus>("tools.actions.getStatus");
      this.post({ type: "fullStatus", ...status });
    } catch {
      // ignore — engine might not be connected
    }
  }

  private post(msg: unknown): void {
    this.view?.webview.postMessage(msg);
  }
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
      margin: 0; padding: 8px 12px;
      font-family: var(--vscode-font-family);
      font-size: var(--vscode-font-size);
      color: var(--vscode-foreground);
      background: var(--vscode-sideBar-background, var(--vscode-editor-background));
    }
    .idle { color: var(--vscode-descriptionForeground); padding: 8px 0; }

    .current {
      padding: 6px 0;
    }
    .current-name {
      font-weight: 600;
      margin-bottom: 4px;
    }
    .current-status {
      color: var(--vscode-descriptionForeground);
      font-size: 0.9em;
      margin-bottom: 4px;
    }
    .progress-bar {
      height: 6px;
      background: var(--vscode-progressBar-background, #0078d4);
      border-radius: 3px;
      transition: width 0.2s ease;
    }
    .progress-track {
      height: 6px;
      background: var(--vscode-input-background);
      border-radius: 3px;
      margin-bottom: 4px;
    }
    .queue-info {
      font-size: 0.85em;
      color: var(--vscode-descriptionForeground);
    }

    .finished-header {
      display: flex;
      justify-content: space-between;
      align-items: center;
      margin-top: 8px;
      padding-top: 6px;
      border-top: 1px solid var(--vscode-panel-border, var(--vscode-sideBar-border));
    }
    .finished-header span {
      font-weight: 600;
      font-size: 0.9em;
    }
    .clear-btn {
      padding: 2px 8px;
      font-size: 0.8em;
      color: var(--vscode-button-foreground);
      background: var(--vscode-button-secondaryBackground);
      border: none; border-radius: 3px;
      cursor: pointer;
    }
    .clear-btn:hover {
      background: var(--vscode-button-secondaryHoverBackground);
    }

    .finished-item {
      display: flex;
      align-items: center;
      gap: 6px;
      padding: 3px 0;
      font-size: 0.9em;
    }
    .finished-icon { font-size: 1.1em; }
    .finished-name { flex: 1; }
    .finished-time {
      color: var(--vscode-descriptionForeground);
      font-size: 0.85em;
    }
  </style>
</head>
<body>
  <div id="root" class="idle">No actions running.</div>
  <script>
    const vscode = acquireVsCodeApi();
    const root = document.getElementById('root');

    let state = {
      busy: false,
      current_name: '',
      progress: 0,
      status: '',
      queued_count: 0,
      finished: []
    };

    vscode.postMessage({ type: 'ready' });

    window.addEventListener('message', (ev) => {
      const m = ev.data;
      switch (m.type) {
        case 'fullStatus':
          state.busy = m.busy;
          state.current_name = m.current_name;
          state.progress = m.progress;
          state.status = m.status;
          state.queued_count = m.queued_count;
          state.finished = m.finished || [];
          render();
          break;
        case 'started':
          state.busy = true;
          state.current_name = m.name;
          state.progress = 0;
          state.status = '';
          render();
          break;
        case 'progress':
          state.progress = m.progress;
          state.status = m.status;
          render();
          break;
        case 'completed':
          state.busy = false;
          state.finished.push({
            name: m.name,
            success: m.success,
            error: m.error,
            duration_ms: m.duration_ms
          });
          render();
          break;
      }
    });

    function render() {
      root.innerHTML = '';
      root.className = '';

      if (state.busy) {
        const cur = document.createElement('div');
        cur.className = 'current';

        const name = document.createElement('div');
        name.className = 'current-name';
        name.textContent = state.current_name || 'Running...';
        cur.appendChild(name);

        if (state.status) {
          const st = document.createElement('div');
          st.className = 'current-status';
          st.textContent = state.status;
          cur.appendChild(st);
        }

        const track = document.createElement('div');
        track.className = 'progress-track';
        const bar = document.createElement('div');
        bar.className = 'progress-bar';
        bar.style.width = Math.round(state.progress * 100) + '%';
        track.appendChild(bar);
        cur.appendChild(track);

        const pct = document.createElement('div');
        pct.className = 'queue-info';
        pct.textContent = Math.round(state.progress * 100) + '%';
        if (state.queued_count > 0) {
          pct.textContent += ' | ' + state.queued_count + ' queued';
        }
        cur.appendChild(pct);

        root.appendChild(cur);
      } else if (state.finished.length === 0) {
        root.className = 'idle';
        root.textContent = 'No actions running.';
        return;
      }

      if (state.finished.length > 0) {
        const hdr = document.createElement('div');
        hdr.className = 'finished-header';
        const title = document.createElement('span');
        title.textContent = 'Completed';
        hdr.appendChild(title);
        const btn = document.createElement('button');
        btn.className = 'clear-btn';
        btn.textContent = 'Clear';
        btn.addEventListener('click', () => {
          vscode.postMessage({ type: 'clearFinished' });
          state.finished = [];
          render();
        });
        hdr.appendChild(btn);
        root.appendChild(hdr);

        for (const r of state.finished) {
          const item = document.createElement('div');
          item.className = 'finished-item';

          const icon = document.createElement('span');
          icon.className = 'finished-icon';
          icon.textContent = r.success ? '✅' : '❌';
          item.appendChild(icon);

          const nm = document.createElement('span');
          nm.className = 'finished-name';
          nm.textContent = r.name;
          if (!r.success && r.error) {
            nm.title = r.error;
          }
          item.appendChild(nm);

          const tm = document.createElement('span');
          tm.className = 'finished-time';
          tm.textContent = formatDuration(r.duration_ms);
          item.appendChild(tm);

          root.appendChild(item);
        }
      }
    }

    function formatDuration(ms) {
      if (ms < 1000) return Math.round(ms) + 'ms';
      if (ms < 60000) return (ms / 1000).toFixed(1) + 's';
      return Math.floor(ms / 60000) + 'm ' + Math.round((ms % 60000) / 1000) + 's';
    }
  </script>
</body>
</html>`;
}
